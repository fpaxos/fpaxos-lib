#include <event2/event.h>
#include <event2/util.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>

#include "evpaxos.h"
#include "tcp_sendbuf.h"
#include "config_reader.h"
#include "tcp_receiver.h"
#include "acceptor_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


struct evacceptor
{
	int acceptor_id;
	struct config* conf;
	struct event_base* base;
	struct tcp_receiver* receiver;
	struct acceptor_state* state;
};


// Received a batch of prepare requests (phase 1a), 
// may answer with multiple messages, all reads/updates
// needs to be wrapped into transactions and made persistent
// before sending the corresponding acknowledgement
static void 
handle_prepare_req(struct evacceptor* a, 
	struct bufferevent* bev, prepare_req* pr)
{
	acceptor_record * rec;	
	LOG(DBG, ("Handling prepare iid %d ballot %d\n", pr->iid, pr->ballot));

	rec = acceptor_state_receive_prepare(a->state, pr);
	
	if (rec != NULL)
		sendbuf_add_prepare_ack(bev, rec, a->acceptor_id);
}

// Received a batch of accept requests (phase 2a)
// may answer with multiple messages, all reads/updates
// needs to be wrapped into transactions and made persistent
// before sending the corresponding acknowledgement
static void 
handle_accept_req(struct evacceptor* a,
	struct bufferevent* bev, accept_req* ar)
{
	LOG(DBG, ("Handling accept for instance %d\n", ar->iid));

	acceptor_record* rec;
	rec = acceptor_state_receive_accept(a->state, ar);
	
	if (rec != NULL) { 	// if accepted, send accept_ack
		int i;
		struct carray* bevs = tcp_receiver_get_events(a->receiver);
		for (i = 0; i < carray_count(bevs); i++) {
			rec->acceptor_id = a->acceptor_id; // TODO needed?
			sendbuf_add_accept_ack(carray_at(bevs, i), rec);
		}
	}
}

// This function is invoked when a new message is ready to be read
// from the acceptor socket	
static void 
handle_req(struct bufferevent* bev, void* arg)
{
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];
	struct evacceptor* a = (struct evacceptor*)arg;
	
	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	evbuffer_remove(in, buffer, msg.data_size);
	
	switch (msg.type) {
		case prepare_reqs:
			handle_prepare_req(a, bev, (prepare_req*)buffer);
			break;
		case accept_reqs:
			handle_accept_req(a, bev, (accept_req*)buffer);
			break;
		default:
		printf("Unknow msg type %d received by acceptor\n", msg.type);
	}
}

struct evacceptor* 
evacceptor_init(int id, const char* config_file, struct event_base* b)
{
	struct evacceptor* a;

	LOG(VRB, ("Acceptor %d starting...\n", id));
		
	// Check that n_of_acceptor is not too big
	if (N_OF_ACCEPTORS >= (sizeof(unsigned int)*8)) {
		printf("Error, this library currently supports at most:%d acceptors\n",
		(int)(sizeof(unsigned int)*8));
		printf("(the number of bits in a 'unsigned int', used as acceptor id)\n");
		return NULL;
	}

	//Check id validity of acceptor_id
	if (id < 0 || id >= N_OF_ACCEPTORS) {
		printf("Invalid acceptor id:%d\n", id);
		return NULL;
	}

	a = malloc(sizeof(struct evacceptor));

	a->conf = read_config(config_file);
	if (a->conf == NULL) {
		free(a);
		return NULL;
	}

    a->acceptor_id = id;
	a->base = b;
	a->receiver = tcp_receiver_new(a->base, &a->conf->acceptors[id],
		handle_req, a);
	a->state = acceptor_state_new(id);
    printf("Acceptor %d is ready\n", id);

    return a;
}

// struct acceptor*
// acceptor_init_recover(int id, const char* config_file, struct event_base* b)
// {
//     //Set recovery mode then start normally
//     storage_do_recovery();
//     return acceptor_init(id, config, b);
// }

int
evacceptor_exit(struct evacceptor* a)
{
	acceptor_state_delete(a->state);
	event_base_loopexit(a->base, NULL);
	return 0;
}

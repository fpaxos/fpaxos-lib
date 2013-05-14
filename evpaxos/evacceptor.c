#include <event2/event.h>
#include <event2/util.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>

#include "evpaxos.h"
#include "tcp_sendbuf.h"
#include "config_reader.h"
#include "tcp_receiver.h"
#include "acceptor.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


struct evacceptor
{
	int acceptor_id;
	struct config* conf;
	struct event_base* base;
	struct tcp_receiver* receiver;
	struct acceptor* state;
};


/*
	Received a prepare request (phase 1a).
*/
static void 
handle_prepare_req(struct evacceptor* a, 
	struct bufferevent* bev, prepare_req* pr)
{
	LOG(DBG, ("Handling prepare for instance %d ballot %d\n", 
		pr->iid, pr->ballot));
	
	acceptor_record * rec;
	rec = acceptor_receive_prepare(a->state, pr);
	sendbuf_add_prepare_ack(bev, rec);
}

/*
	Received a accept request (phase 2a).
*/
static void 
handle_accept_req(struct evacceptor* a,
	struct bufferevent* bev, accept_req* ar)
{
	LOG(DBG, ("Handling accept for instance %d ballot %d\n",
	ar->iid, ar->ballot));

	int i;
	struct carray* bevs = tcp_receiver_get_events(a->receiver);
	acceptor_record* rec = acceptor_receive_accept(a->state, ar);
	rec->acceptor_id = a->acceptor_id; // TODO id should already be set!
	if (ar->ballot == rec->ballot) // accepted!
		for (i = 0; i < carray_count(bevs); i++)
			sendbuf_add_accept_ack(carray_at(bevs, i), rec);
	else
		sendbuf_add_accept_ack(bev, rec); // send nack
}

/*
	This function is invoked when a new message is ready to be read.
*/
static void 
handle_req(struct bufferevent* bev, void* arg)
{
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];
	struct evacceptor* a = (struct evacceptor*)arg;
	
	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	if (msg.data_size > PAXOS_MAX_VALUE_SIZE)
	{
		evbuffer_drain(in, msg.data_size);
		printf("Acceptor received req sz %d > %d maximum, discarding\n",
			msg.data_size, PAXOS_MAX_VALUE_SIZE);
		
		return;
	}
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
		
	// Check id validity of acceptor_id
	if (id < 0 || id >= N_OF_ACCEPTORS) {
		printf("Invalid acceptor id:%d\n", id);
		printf("Should be between 0 and %d\n", N_OF_ACCEPTORS);
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
	a->state = acceptor_new(id);
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
	acceptor_delete(a->state);
	return 0;
}

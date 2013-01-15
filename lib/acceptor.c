#include <event2/event.h>
#include <event2/util.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>

#include "libpaxos.h"
#include "libpaxos_priv.h"
#include "paxos_net.h"
#include "storage.h"
#include "config_reader.h"
#include "tcp_receiver.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


struct acceptor
{
	struct config* conf;
	int acceptor_id;
	struct storage* store;
	struct event_base* base;
	struct tcp_receiver* receiver;
};


/*-------------------------------------------------------------------------*/
// Helpers
/*-------------------------------------------------------------------------*/

// Given an accept request (phase 2a) message and the current record
// will update the record if the request is legal
// Return NULL for no changes, the new record if the accept was applied
static acceptor_record *
acc_apply_accept(struct acceptor* a, accept_req * ar, acceptor_record * rec)
{
    //We already have a more recent ballot
    if (rec != NULL && rec->ballot < ar->ballot) {
        LOG(DBG, ("Accept for iid:%u dropped (ballots curr:%u recv:%u)\n", 
            ar->iid, rec->ballot, ar->ballot));
        return NULL;
    }
    
    //Record not found or smaller ballot
    // in both cases overwrite and store
    LOG(DBG, ("Accepting for iid:%u (ballot:%u)\n", 
        ar->iid, ar->ballot));
    
    //Store the updated record
    rec = storage_save_accept(a->store, ar);
    
    return rec;
}

//Given a prepare (phase 1a) request message and the
// corresponding record, will update if the request is valid
// Return NULL for no changes, the new record if the promise was made
static acceptor_record *
acc_apply_prepare(struct acceptor* a, prepare_req * pr, acceptor_record * rec)
{
    //We already have a more recent ballot
    if (rec != NULL && rec->ballot >= pr->ballot) {
        LOG(DBG, ("Prepare request for iid:%u dropped (ballots curr:%u recv:%u)\n", 
            pr->iid, rec->ballot, pr->ballot));
        return NULL;
    }
    
    //Stored value is final, the instance is closed already
    if (rec != NULL && rec->is_final) {
        LOG(DBG, ("Prepare request for iid:%u dropped \
            (stored value is final)\n", pr->iid));
        return NULL;
    }
    
    //Record not found or smaller ballot
    // in both cases overwrite and store
    LOG(DBG, ("Prepare request is valid for iid:%u (ballot:%u)\n", 
        pr->iid, pr->ballot));
    
    //Store the updated record
    rec = storage_save_prepare(a->store, pr, rec);

    return rec;
}


/*-------------------------------------------------------------------------*/
// Event handlers
/*-------------------------------------------------------------------------*/

//Received a batch of prepare requests (phase 1a), 
// may answer with multiple messages, all reads/updates
// needs to be wrapped into transactions and made persistent
// before sending the corresponding acknowledgement
static void 
handle_prepare_req(struct acceptor* a, struct bufferevent* bev, prepare_req* pr)
{
	acceptor_record * rec;
	
    LOG(DBG, ("Handle prepare request for instance %d ballot %d\n", pr->iid, pr->ballot));

    // Wrap changes in a transaction
    storage_tx_begin(a->store);
	
	// Retrieve corresponding record
	rec = storage_get_record(a->store, pr->iid);
	// Try to apply prepare
	rec = acc_apply_prepare(a, pr, rec);
	// If accepted, send accept_ack
    storage_tx_commit(a->store);

	if (rec != NULL)
		sendbuf_add_prepare_ack(bev, rec, a->acceptor_id);
}

//Received a batch of accept requests (phase 2a)
// may answer with multiple messages, all reads/updates
// needs to be wrapped into transactions and made persistent
// before sending the corresponding acknowledgement
static void 
handle_accept_req(struct acceptor* a, struct bufferevent* bev, accept_req* ar)
{
    LOG(DBG, ("Handling accept for instance %d\n", ar->iid));

	acceptor_record* rec;
	
    // Wrap in a transaction
    storage_tx_begin(a->store);
	// Retrieve corresponding record
	rec = storage_get_record(a->store, ar->iid);
	// Try to apply accept
	rec = acc_apply_accept(a, ar, rec);
	
	storage_tx_commit(a->store);

	// If accepted, send accept_ack
	if (rec != NULL) {
		rec->acceptor_id = a->acceptor_id;
		sendbuf_add_accept_ack(a->receiver, rec);
	}
}

//This function is invoked when a new message is ready to be read
// from the acceptor UDP socket	
static void 
handle_req(struct bufferevent* bev, void* arg)
{
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];
	struct acceptor* a = (struct acceptor*)arg;
	
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


/*-------------------------------------------------------------------------*/
// Public functions (see libpaxos.h for more details)
/*-------------------------------------------------------------------------*/
struct acceptor* 
acceptor_init(int id, const char* config_file, struct event_base* b)
{
	struct acceptor* a;

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

	a = malloc(sizeof(struct acceptor));
  	
	a->conf = read_config(config_file);
	if (a->conf == NULL) {
		free(a);
		return NULL;
	}

    a->acceptor_id = id;

    // Initialize BDB 
	a->store = storage_open(id, 0);
    if (a->store == NULL) {
		printf("Acceptor stable storage init failed\n");
		free(a);
		return NULL;
    }

	a->base = b;
	a->receiver = tcp_receiver_new(a->base,
		&a->conf->acceptors[id], handle_req, a);
	
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
acceptor_exit(struct acceptor* a)
{
    if (storage_close(a->store) != 0) {
        printf("storage shutdown failed!\n");
    }
	event_base_loopexit(a->base, NULL);
    return 0;
}

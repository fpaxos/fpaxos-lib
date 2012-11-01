#include <event2/event.h>
#include <event2/util.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>

#include "libpaxos.h"
#include "libpaxos_priv.h"
#include "paxos_net.h"
#include "acceptor_stable_storage.h"
#include "config_reader.h"
#include "tcp_receiver.h"

#include <stdio.h>
#include <assert.h>


//Unique identifier of this acceptor
static int this_acceptor_id = -1;

//UDP socket managers for sending
// static udp_send_buffer * to_learners;

//Event: Time to repeat last accept
// static struct event repeat_accept_event;
//Interval at which the previous event fires
// static struct timeval periodic_repeat_interval;

//The highest instance id for which a value was accepted
static iid_t highest_accepted_iid = 0;

static config* conf;
static struct event_base* base;
static struct tcp_receiver* receiver;

// TODO periodic retransmission and update-on-deliver are currently in a transaction. Could be prepended to the next instead

/*-------------------------------------------------------------------------*/
// Helpers
/*-------------------------------------------------------------------------*/

//Given an accept request (phase 2a) message and the current record
// will update the record if the request is legal
// Return NULL for no changes, the new record if the accept was applied
static acceptor_record *
acc_apply_accept(accept_req * ar, acceptor_record * rec) {
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
    rec = stablestorage_save_accept(ar);
    
    //Keep track of highest accepted for retransmission
    if(ar->iid > highest_accepted_iid) {
        highest_accepted_iid = ar->iid;
        LOG(DBG, ("Highest accepted is now iid:%u\n", 
            highest_accepted_iid));
    }
    return rec;
}

//Given a prepare (phase 1a) request message and the
// corresponding record, will update if the request is valid
// Return NULL for no changes, the new record if the promise was made
static acceptor_record *
acc_apply_prepare(prepare_req * pr, acceptor_record * rec) {
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
    rec = stablestorage_save_prepare(pr, rec);

    return rec;
}

//Reads the last (by iid) instance for which a value was accepted
// and re-transmit it to the learners
// static void
// acc_retransmit_latest_accept() {
//     
//     acceptor_record * rec;
//     
//     //Fetch the highest instance accepted
//     sendbuf_clear(to_learners, accept_acks, this_acceptor_id);
//     stablestorage_tx_begin();
//     rec = stablestorage_get_record(highest_accepted_iid);
//     
//     //And retransmit it to learners
//     sendbuf_add_accept_ack(to_learners, rec);
//     stablestorage_tx_end();
//     
//     sendbuf_flush(to_learners);   
// }
 
//This function is invoked periodically
// (periodic_repeat_interval) to retrasmit the most 
// recent accept
// static void
// acc_periodic_repeater(int fd, short event, void *arg)
// {
//     UNUSED_ARG(fd);
//     UNUSED_ARG(event);
//     UNUSED_ARG(arg);
//     
//     //If some value has been accepted,
//     if (highest_accepted_iid > 0) {
//         //Rebroadcast most recent (so that learners stay up-to-date)
//         LOG(DBG, ("re-sending most recent accept, iid:%u\n", highest_accepted_iid));
//         acc_retransmit_latest_accept();
//     }
//     
//     //Set the next timeout for calling this function
//     if(event_add(&repeat_accept_event, &periodic_repeat_interval) != 0) {
// 	   printf("Error while adding next repeater periodic event\n");
// 	}
// }


/*-------------------------------------------------------------------------*/
// Event handlers
/*-------------------------------------------------------------------------*/

//Received a batch of prepare requests (phase 1a), 
// may answer with multiple messages, all reads/updates
// needs to be wrapped into transactions and made persistent
// before sending the corresponding acknowledgement
static void 
handle_prepare_req(struct bufferevent* bev, prepare_req* pr)
{
	acceptor_record * rec;
    LOG(DBG, ("Handle prepare request for instance %d ballot %d\n", pr->iid, pr->ballot));

    // Wrap changes in a transaction
    stablestorage_tx_begin();
	
	// Retrieve corresponding record
	rec = stablestorage_get_record(pr->iid);
	// Try to apply prepare
	rec = acc_apply_prepare(pr, rec);
	// If accepted, send accept_ack
    stablestorage_tx_end();

	if (rec != NULL)
		sendbuf_add_prepare_ack(bev, rec, this_acceptor_id);
}

//Received a batch of accept requests (phase 2a)
// may answer with multiple messages, all reads/updates
// needs to be wrapped into transactions and made persistent
// before sending the corresponding acknowledgement
static void 
handle_accept_req(struct bufferevent* bev, accept_req* ar) {
    LOG(DBG, ("Handling accept for instance %d\n", ar->iid));

	acceptor_record* rec;
	
    // Wrap in a transaction
    stablestorage_tx_begin();
	// Retrieve correspondin record
	rec = stablestorage_get_record(ar->iid);
	// Try to apply accept
	rec = acc_apply_accept(ar, rec);
	
	stablestorage_tx_end();

	// If accepted, send accept_ack
	if (rec != NULL) {
		rec->acceptor_id = this_acceptor_id;
		sendbuf_add_accept_ack(receiver, rec);
	}
}

//Received a batch of repeat requests from a learner,
// will repeat the accepted value for that instance (if any).
// May answer with multiple messages, all reads are wrapped
// into transactions
// static void 
// handle_repeat_req_batch(repeat_req_batch* rrb) {
//     LOG(DBG, ("Repeating accept for %d instances\n", rrb->count));
// 
//     //Create empty accept_ack_batch in buffer
//     sendbuf_clear(to_learners, accept_acks, this_acceptor_id);
// 
//     //Wrap in a (read-only) transaction
//     stablestorage_tx_begin();
//     
//     short int i;
//     acceptor_record * rec;
//     
//     //Iterate over the repeat_req in the batch
//     for(i = 0; i < rrb->count; i++) {
//         //Read the corresponding record
//         rec = stablestorage_get_record(rrb->requests[i]);
//         
//         //If a value was accepted, send accept_ack
//         if(rec != NULL && rec->value_size > 0) {
//             sendbuf_add_accept_ack(to_learners, rec);
//         } else {
//             LOG(DBG, ("Cannot retransmit iid:%u no value accepted \n", rrb->requests[i]));
//         }
//     }
//     
//     stablestorage_tx_end();
//     
//     //Flush the send buffer if there's something
//     sendbuf_flush(to_learners);
// }

//This function is invoked when a new message is ready to be read
// from the acceptor UDP socket	
static void acc_handle_newmsg(struct bufferevent* bev, void* arg) {
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];

	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	evbuffer_remove(in, buffer, msg.data_size);
	
    switch (msg.type) {
        case prepare_reqs:
            handle_prepare_req(bev, (prepare_req*)buffer);
        	break;
        case accept_reqs:
            handle_accept_req(bev, (accept_req*)buffer);
        	break;
        // case repeat_reqs:
        //     handle_repeat_req_batch((repeat_req_batch*)buffer);
        // 	break;
        default:
            printf("Unknow msg type %d received by acceptor\n", msg.type);
    }
}
	
//The acceptor runs on top of a learner, if the learner is active
// (ACCEPTOR_UPDATE_ON_DELIVER is defined), this is the function 
// invoked when a value is delivered.
// The acceptor overwrites his personal record with the delivered value
// since it will never change again
void 
acc_deliver_callback(char * value, size_t size, iid_t iid, ballot_t ballot, int proposer) {
    UNUSED_ARG(proposer);

#ifndef ACCEPTOR_UPDATE_ON_DELIVER
    UNUSED_ARG(value);
    UNUSED_ARG(size);
    UNUSED_ARG(iid);
    UNUSED_ARG(ballot);

    //ACCEPTOR_UPDATE_ON_DELIVER not defined, 
    //this callback should not even be called!
    printf("Warning:%s invoked when ACCEPTOR_UPDATE_ON_DELIVER is undefined!\n", __func__);

#else
    //Save permanently the value delivered, replacing the
    // accept for this particular acceptor
    //FIXME: Could append to next TX instead of doing a separate one
    stablestorage_tx_begin();
    stablestorage_save_final_value(value, size, iid, ballot);
    stablestorage_tx_end();
#endif
}

/*-------------------------------------------------------------------------*/
// Initialization
/*-------------------------------------------------------------------------*/

//Initialize sockets and related events
// static int 
// init_acc_network() {	
//     // Send buffer for talking to proposers
//     to_proposers = sendbuf_new(conf->proposers_count, conf->proposers);
//     if(to_proposers == NULL) {
//         printf("Error creating acceptor->proposers network sender\n");
//         return ACCEPTOR_ERROR;
//     }
// 
//     // Send buffer for talking to learners
//     to_learners = sendbuf_new(conf->learners_count, conf->learners);
//     if(to_learners == NULL) {
//         printf("Error creating acceptor->learners network sender\n");
//         return ACCEPTOR_ERROR;
//     }
//     
//     // Message receive event
//     for_acceptor = udp_receiver_new(&conf->acceptors[this_acceptor_id]);
//     if (for_acceptor == NULL) {
//         printf("Error creating acceptor network receiver\n");
//         return ACCEPTOR_ERROR;
//     }
// 
//     event_set(&acceptor_msg_event, for_acceptor->sock, EV_READ|EV_PERSIST, acc_handle_newmsg, NULL);
//     event_add(&acceptor_msg_event, NULL);
//     
//     return 0;
// }

//Initialize timers
// static int 
// init_acc_timers() {
//     
//     //Sets the first acc_periodic_repeater invocation timeout
//     evtimer_set(&repeat_accept_event, acc_periodic_repeater, NULL);
// 	evutil_timerclear(&periodic_repeat_interval);
// 	periodic_repeat_interval.tv_sec = ACCEPTOR_REPEAT_INTERVAL;
//     periodic_repeat_interval.tv_usec = 0;
// 	if (event_add(&repeat_accept_event, &periodic_repeat_interval) != 0) {
// 	   printf("Error while adding first periodic repeater event\n");
//        return -1;
// 	}
//     return 0;
// }

//Initialize the underlying persistent storage
// Berlekey DB in this case
// static int
// init_acc_stable_storage() {
    // return ;
// }

//Acceptor initialization, this function is invoked by
// the underlying learner after it's normal initialization
// static int init_acceptor() {

// #ifdef ACCEPTOR_UPDATE_ON_DELIVER
    //Keep the learnern running as normal
    //Will deliver values when decided
    // LOG(VRB, ("Acceptor will update stored values as they are delivered\n"));
// #else
    //Shut down the underlying learner,
    //(but keep using it's event loop)
    // LOG(VRB, ("Acceptor shutting down learner events\n"));
    // learner_suspend();
// #endif
    
    // //Add network events and prepare send buffer
    // if(init_acc_network() != 0) {
    //     printf("Acceptor network init failed\n");
    //     return -1;
    // }
    // 
    // //Add additional timers to libevent loop
    // if(init_acc_timers() != 0){
    //     printf("Acceptor timers init failed\n");
    //     return -1;
    // }
    // 
    // //Initialize BDB 
    // if(init_acc_stable_storage() != 0) {
    //     printf("Acceptor stable storage init failed\n");
    //     return -1;
    // }
    // return 0;
// }

/*-------------------------------------------------------------------------*/
// Public functions (see libpaxos.h for more details)
/*-------------------------------------------------------------------------*/

int acceptor_init(int acceptor_id, const char* config_file) {	
    // Check that n_of_acceptor is not too big
    if (N_OF_ACCEPTORS >= (sizeof(unsigned int)*8)) {
        printf("Error, this library currently supports at most:%d acceptors\n",
            (int)(sizeof(unsigned int)*8));
        printf("(the number of bits in a 'unsigned int', used as acceptor id)\n");
        return -1;
    }
    
    //Check id validity of acceptor_id
    if (acceptor_id < 0 || acceptor_id >= N_OF_ACCEPTORS) {
        printf("Invalid acceptor id:%d\n", acceptor_id);
        return -1;
    }

	conf = read_config(config_file);
	if (conf == NULL)
		return -1;
    
    this_acceptor_id = acceptor_id;

    //Add additional timers to libevent loop
    // if (init_acc_timers() != 0){
        // printf("Acceptor timers init failed\n");
        // return -1;
    // }
    
    //Initialize BDB 
    if (stablestorage_init(this_acceptor_id) != 0) {
        printf("Acceptor stable storage init failed\n");
        return -1;
    }

    LOG(VRB, ("Acceptor %d starting...\n", this_acceptor_id));

    
    // //Starts a learner with a custom init function
    // if (learner_init(acceptor_id, config_file, acc_deliver_callback, init_acceptor) != 0) {
    //     printf("Could not start the learner!\n");
    //     return -1;
    // }
	
	base = event_base_new();
	receiver = tcp_receiver_new(base, 
		&conf->acceptors[this_acceptor_id], acc_handle_newmsg, NULL);
	
    printf("Acceptor %d is ready\n", this_acceptor_id);

	event_base_dispatch(base);
    return 0;
}

int acceptor_init_recover(int acceptor_id, const char* config_file) {
    //Set recovery mode then start normally
    stablestorage_do_recovery();
    return acceptor_init(acceptor_id, config_file);
}

int acceptor_exit() {
    if (stablestorage_shutdown() != 0) {
        printf("stablestorage shutdown failed!\n");
    }
	event_base_loopexit(base, NULL);
    return 0;
}

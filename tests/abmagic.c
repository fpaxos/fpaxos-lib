#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#include <db.h>

#include "event.h"
#include "evutil.h"

#include "libpaxos.h"
#include "libpaxos_priv.h"
#include "paxos_net.h"
#include "acceptor_stable_storage.h"

//Interval for forcing buffer flush in microseconds
#define ABMAGIC_FLUSH_INTERVAL 2000000

//Libevent handle
static struct event_base * eb;
// Event: a client value was received
struct event client_msg_event;
// Event: a repeat_request was received
struct event repeat_msg_event;
// Event: time to flush send buffer
static struct event flush_event;
//Time interval for the previous event
static struct timeval flush_interval;

//Network managers
static udp_send_buffer * to_learners;
// static udp_send_buffer * to_learners;
static udp_receiver * from_learners;
static udp_receiver * from_clients;

static iid_t current_iid = 1;
static accept_ack * accept_buffer;

// FIXME: should store current_iid when committing
// but with recno is a mess
// Cannot use 0 and does not like a large number either...
extern DB *dbp;
extern DB_TXN *txn;

static void
ab_store_value(char * value, size_t value_size) {

    accept_buffer->iid = current_iid;
    accept_buffer->ballot = 101;
    accept_buffer->value_ballot = 101;
    accept_buffer->is_final = 1;
    accept_buffer->value_size = value_size;
    memcpy(accept_buffer->value, value, value_size);
    
    //Store as acceptor_record (== accept_ack)
    stablestorage_save_final_value(value, value_size, current_iid, 101);
}
// 
// static void
// ab_add_to_send_buffer(accept_ack * aa) {
//     paxos_msg * m = (paxos_msg *) &to_learners->buffer;
//     assert(m->type == accept_acks);
// 
//     accept_ack_batch * aab = (accept_ack_batch *)&m->data;
//     
//     size_t aa_size = ACCEPT_ACK_SIZE(accept_buffer);
//     
//     if(PAXOS_MSG_SIZE(m) + aa_size >= MAX_UDP_MSG_SIZE) {
//         // Next accept to add does not fit, flush the current 
//         // message before adding it
//         stablestorage_tx_end();
//         sendbuf_flush(to_learners);
//         sendbuf_clear(to_learners, m->type, 0);
//         stablestorage_tx_begin();   
//     }
//     printf("bufsize%d\n", (int)(PAXOS_MSG_SIZE(m) + aa_size));
// 
//     accept_ack * new_aa = (accept_ack *)&m->data[m->data_size];
//     memcpy(new_aa, aa, aa_size);
//     
//     to_learners->dirty = 1;
//     m->data_size += aa_size;
//     aab->count += 1;
// }

static void 
ab_handle_newval(int sock, short event, void *arg) {
    //Make the compiler happy!
    UNUSED_ARG(sock);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    
    assert(sock == from_clients->sock);
    //Read the next message
    int valid = udp_read_next_message(from_clients);
    if (valid < 0) {
        printf("Dropping invalid client message\n");
        return;
    }

    //The message is valid, take the appropriate action
    // based on the type
    paxos_msg * msg = (paxos_msg*) &from_clients->recv_buffer;
    switch(msg->type) {
        
        case submit: {
            ab_store_value(msg->data, msg->data_size);
            sendbuf_add_accept_ack(to_learners, accept_buffer);
            current_iid +=1;
        }
        break;

        default: {
            printf("Unknow msg type %d received\n", msg->type);
        }
    }
}

static void
ab_handle_repeat_req_batch(repeat_req_batch * rrb) {
    LOG(DBG, ("Repeating accept for %d instances\n", rrb->count));

    //Create empty accept_ack_batch in buffer
    sendbuf_clear(to_learners, accept_acks, 0);

    short int i;
    accept_ack * aa;
    
    //Iterate over the repeat_req in the batch
    for(i = 0; i < rrb->count; i++) {
        //Read the corresponding record
        aa = stablestorage_get_record(rrb->requests[i]);
        
        //If a value was accepted, send accept_ack
        if(aa != NULL && aa->value_size > 0) {
            sendbuf_add_accept_ack(to_learners, aa);
        } else {
            LOG(DBG, ("Cannot retransmit iid:%u no value accepted \n", aa->iid));
        }
    }
    //Flush the send buffer if there's something
    sendbuf_flush(to_learners);
}


static void 
ab_handle_leamsg(int sock, short event, void *arg) {
    //Make the compiler happy!
    UNUSED_ARG(sock);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    
    assert(sock == from_learners->sock);
    //Read the next message
    int valid = udp_read_next_message(from_learners);
    if (valid < 0) {
        printf("Dropping invalid learner message\n");
        return;
    }

    //The message is valid, take the appropriate action
    // based on the type
    paxos_msg * msg = (paxos_msg*) &from_learners->recv_buffer;
    switch(msg->type) {

        case repeat_reqs: {
            ab_handle_repeat_req_batch((repeat_req_batch*) msg->data);
        }
        break;

        default: {
            printf("Unknow msg type %d received\n", msg->type);
        }
    }
}

static void 
ab_periodic_flush(int sock, short event, void *arg) {
    //Make the compiler happy!
    UNUSED_ARG(sock);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    
    stablestorage_tx_end();   
    sendbuf_flush(to_learners);
    stablestorage_tx_begin();
    
    if(event_add(&flush_event, &flush_interval) != 0) {
	   printf("Error while adding first periodic flush event\n");
	}
}

static int 
ab_init() {
    
    accept_buffer = malloc(MAX_UDP_MSG_SIZE);
    if(accept_buffer == NULL) {
        printf("Error in malloc\n");
        return -1;        
    }
    
    //Init BDB
    if(stablestorage_init(0) != 0) {
        printf("Stable storage init failed\n");
    }
    //Start the first transaction
    stablestorage_tx_begin();


    //Initialization of libevent handle
    if((eb = event_init()) == NULL) {
        printf("Error in libevent init\n");
        return -1;
    }
    
    // Send buffer for talking to learners
    to_learners = udp_sendbuf_new(PAXOS_LEARNERS_NET);
    if(to_learners == NULL) {
        printf("Error creating network sender\n");
        return -1;
    }
    sendbuf_clear(to_learners, accept_acks, 0);


    // // Send buffer for talking to learners 
    // // (dedicated to answering repeat request)
    // to_learners = udp_sendbuf_new(PAXOS_LEARNERS_NET);
    // if(to_learners == NULL) {
    //     printf("Error creating network sender 2\n");
    //     return -1;
    // }

    // Message from learners (repeat request) event
    from_learners = udp_receiver_new(PAXOS_ACCEPTORS_NET);
    if (from_learners == NULL) {
        printf("Error creating network receiver\n");
        return -1;
    }
    event_set(&repeat_msg_event, from_learners->sock, EV_READ|EV_PERSIST, ab_handle_leamsg, NULL);
    event_add(&repeat_msg_event, NULL);

    
    // Message from client event
    from_clients = udp_receiver_new(PAXOS_SUBMIT_NET);
    if (from_clients == NULL) {
        printf("Error creating network receiver\n");
        return -1;
    }
    event_set(&client_msg_event, from_clients->sock, EV_READ|EV_PERSIST, ab_handle_newval, NULL);
    event_add(&client_msg_event, NULL);
    
    // Periodic event to flush send buffer
    evtimer_set(&flush_event, ab_periodic_flush, NULL);
	evutil_timerclear(&flush_interval);
	flush_interval.tv_sec = ABMAGIC_FLUSH_INTERVAL / 1000000;
    flush_interval.tv_usec = ABMAGIC_FLUSH_INTERVAL % 1000000;
	if(event_add(&flush_event, &flush_interval) != 0) {
	   printf("Error while adding first periodic flush event\n");
       return -1;
	}
	    
    printf("ABMagic is ready!\n");
    return 0;
}


int main (int argc, char const *argv[]) {    
    if (ab_init() != 0) {
        printf("Init failed!\n");
        return -1;
    }

    //Event loop, does not return
    event_dispatch();
    return 0;
    
    argc = argc;
    argv = argv;
}
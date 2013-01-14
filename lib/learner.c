#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <memory.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "libpaxos.h"
#include "libpaxos_priv.h"
#include "paxos_net.h"
#include "config_reader.h"

#define INST_INFO_EMPTY (0)
#define IS_CLOSED(INST) (INST->final_value != NULL)

//Structure used to store all info relative to a given instance
struct instance
{
    iid_t           iid;
    ballot_t        last_update_ballot;
    accept_ack*     acks[N_OF_ACCEPTORS];
    accept_ack*     final_value;
};

struct learner
{
	//Highest instance for which a message was seen
	iid_t highest_iid_seen; 
	// Current instance, incremented when current is closed and 
	// the corresponding value is delivered
	iid_t current_iid;
	// Highest instance that is already closed 
	// (can be higher than current!)
	// TODO: not used
	iid_t highest_iid_closed; 
	// Array (used as a circular buffer) to store instance infos
	struct instance instances[LEARNER_ARRAY_SIZE];
	// Function to invoke when the current_iid is closed, 
	// the final value and some other informations is passed as argument
	deliver_function delfun;
	// Argument to deliver function
	void* delarg;
	// libevent handle
	struct event_base * base;
	// config reader handle
 	struct config* conf;
	// bufferevent sockets to send data to acceptors
	struct bufferevent* acceptor_ev[N_OF_ACCEPTORS];
};

/*-------------------------------------------------------------------------*/
// Helpers
/*-------------------------------------------------------------------------*/

static struct instance*
learner_get_instance(struct learner* l, iid_t iid)
{
	// This is equivalent to n mod LEARNER_ARRAY_SIZE, 
	// works only if LEARNER_ARRAY_SIZE is a power of 2.
	
	return &(l->instances[iid & (LEARNER_ARRAY_SIZE-1)]);
}

// Reset all fields and free stored messages
// of a learner instance
static void
learner_clear_instance(struct learner* l, iid_t iid)
{
	int i;
	struct instance* ii;
	
	ii = learner_get_instance(l, iid);
    ii->iid = INST_INFO_EMPTY;
    ii->last_update_ballot = 0;
    ii->final_value = NULL;

    // Free all stored accept_ack
    for (i = 0; i < N_OF_ACCEPTORS; i++) {
        if (ii->acks[i] != NULL) {
            PAX_FREE(ii->acks[i]);
            ii->acks[i] = NULL;            
        }
    }
}

// Stores the accept_ack of a given acceptor in the corresponding record 
// at the appropriate index
// Assumes rec->acks[acc_id] is NULL already
static void 
instance_store_accept(struct instance* ii, accept_ack * aa)
{
	accept_ack * new_ack;
	
	new_ack = PAX_MALLOC(ACCEPT_ACK_SIZE(aa));
    memcpy(new_ack, aa, ACCEPT_ACK_SIZE(aa));
    //Store message at appropriate index (acceptor_id)
    ii->acks[aa->acceptor_id] = new_ack;
    //Keep track of most recent accept_ack stored
    ii->last_update_ballot = aa->ballot;
}

// Tries to update the state based on the accept_ack received.
// Returns 0 if the message was discarded because not relevant. 1 if the state changed.
static int
instance_update(struct instance* ii, accept_ack * aa)
{
    // First message for this iid
    if (ii->iid == INST_INFO_EMPTY) {
        LOG(DBG, ("Received first message for instance:%u\n", aa->iid));
        ii->iid = aa->iid;
        ii->last_update_ballot = aa->ballot;
    }
    assert(ii->iid == aa->iid);
    
    // Instance closed already, drop
    if (IS_CLOSED(ii)) {
        LOG(DBG, ("Dropping accept_ack for iid:%u, already closed\n", aa->iid));
        return 0;
    }
    
    // No previous message to overwrite for this acceptor
    if (ii->acks[aa->acceptor_id] == NULL) {
        LOG(DBG, ("Got first ack for iid:%u, acceptor:%d\n", \
            ii->iid, aa->acceptor_id));
        //Save this accept_ack
        instance_store_accept(ii, aa);
        return 1;
    }
    
    //There is already a message from the same acceptor
    accept_ack* prev_ack = ii->acks[aa->acceptor_id];
    
    // Already more recent info in the record, accept_ack is old
    if (prev_ack->ballot >= aa->ballot) {
        LOG(DBG, ("Dropping accept_ack for iid:%u, stored ballot is newer or equal\n", aa->iid));
        return 0;
    }
    
    //Replace the previous ack since the received ballot is newer
    LOG(DBG, ("Overwriting previous accept_ack for iid:%u\n", aa->iid));
    PAX_FREE(prev_ack);
    instance_store_accept(ii, aa);

    return 1;
}

//Checks if a given instance is closed, that is if a quorum of acceptor
// accepted the same value+ballot
//Returns 1 if the instance is closed, 0 otherwise
static int 
instance_has_quorum(struct instance* ii)
{
    size_t i, a_valid_index = -1, count = 0;
    accept_ack * curr_ack;
    
    //Iterates over stored acks
    for (i = 0; i < N_OF_ACCEPTORS; i++) {
        curr_ack = ii->acks[i];
        
        // skip over missing acceptor acks
        if (curr_ack == NULL)
            continue;
        
        // Count the ones "agreeing" with the last added
        if (curr_ack->ballot == ii->last_update_ballot) {
            count++;
            a_valid_index = i;
            
            // Special case: an acceptor is telling that
            // this value is -final-, it can be delivered immediately.
            if (curr_ack->is_final) {
                //For sure >= than quorum...
                count += N_OF_ACCEPTORS;
                break;
            }
        }
    }
    
    //Reached a quorum/majority!
    if (count >= QUORUM) {
        LOG(DBG, ("Reached quorum, iid:%u is closed!\n", ii->iid));
        ii->final_value = ii->acks[a_valid_index];
        return 1;
    }
    
    //No quorum yet...
    return 0;
}

//Invoked when the current_iid is closed.
// Since other instances may be closed too (curr+1, curr+2), also tries to deliver them
static void
learner_deliver_next_closed(struct learner* l)
{
    //Get next instance (last delivered + 1)
    accept_ack * aa;
	int proposer_id;
    struct instance * ii = learner_get_instance(l, l->current_iid);

	// deliver all subsequent instances
    while (IS_CLOSED(ii)) {
        assert(ii->iid == l->current_iid);
        aa = ii->final_value;
        
        //Deliver the value trough callback
		proposer_id = aa->ballot % MAX_N_OF_PROPOSERS;
		
        l->delfun(aa->value, aa->value_size, l->current_iid, 
			aa->ballot, proposer_id, l->delarg);
        
        //Clear the state
		learner_clear_instance(l, ii->iid);
        
        //Move to next instance
        l->current_iid++;
		ii = learner_get_instance(l, l->current_iid);
    }
}

// //Creates a batch of repeat_req to send to the acceptor, 
// // asking to retransmit their accepted value for a given instance
// static void 
// lea_send_repeat_request(iid_t from, iid_t to) {
//     iid_t i;
//     
//     l_inst_info * ii;
//     //Create empty repeat_request in buffer
//     sendbuf_clear(to_acceptors, repeat_reqs, -1);
//     
//     for(i = from; i < to; i++) {
//         //Request all non-closed in from...to range
//         ii = GET_LEA_INSTANCE(i);
//         if(!IS_CLOSED(ii)) {
//             sendbuf_add_repeat_req(to_acceptors, i);
//         }
//     }
//     //Flush if dirty flag is set
//     sendbuf_flush(to_acceptors);
// }

//This function is invoked periodically and tries to detect if the learner 
// missed some message. For example if instance I is closed but instance I-1 
// it's not, we can't deliver I. So it will ask to the acceptors to repeat 
// their accepted value
// static void
// lea_hole_check(int fd, short event, void *arg) {
//     //Periodic check for missing instances
//     //(i.e. i+1 closed, but i not closed yet)
//     if (highest_iid_seen > current_iid + LEARNER_ARRAY_SIZE) {
//         LOG(0, ("This learner is lagging behind!!!, highest seen:%u, highest delivered:%u\n", 
//             highest_iid_seen, current_iid-1));
//         lea_send_repeat_request(current_iid, highest_iid_seen);
//     } else if (highest_iid_closed > current_iid) {
//         LOG(VRB, ("Out of sync, highest closed:%u, highest delivered:%u\n", 
//             highest_iid_closed, current_iid-1));
//         //Ask retransmission to acceptors
//         lea_send_repeat_request(current_iid, highest_iid_closed);
//     }
// 
//     //Set the next timeout for calling this function
//     if (event_add(&hole_check_event, &hole_check_interval) != 0) {
// 	   printf("Error while adding next hole_check event\n");
// 	}
// }

/*-------------------------------------------------------------------------*/
// Event handlers
/*-------------------------------------------------------------------------*/

// Called when an accept_ack is received, the learner will update it's status
// for that instance and afterward check if the instance is closed
static void
learner_handle_accept_ack(struct learner* l, accept_ack * aa)
{
	int relevant;
	struct instance* ii;
	
	// Keep track of highest seen instance id
	if (aa->iid > l->highest_iid_seen) {
		l->highest_iid_seen = aa->iid;
	}

	//Already closed and delivered, ignore message
	if (aa->iid < l->current_iid) {
		LOG(DBG, ("Dropping accept_ack for already delivered iid:%u\n", aa->iid));
		return;
	}

	//We are late w.r.t the current iid, ignore message
	// (The instance received is too ahead and will overwrite something)
	if (aa->iid >= l->current_iid + LEARNER_ARRAY_SIZE) {
		LOG(DBG, ("Dropping accept_ack for iid:%u, too far in future\n", aa->iid));
		return;
	}

	//Message is within interesting bounds
	//Update the corresponding record

	ii = learner_get_instance(l, aa->iid);
	relevant = instance_update(ii, aa);

	if (!relevant) {
		//Not really interesting (i.e. a duplicate message)
		LOG(DBG, ("Learner discarding learn for iid:%u\n", aa->iid));
		return;
	}

	// Message contained some relevant info, 
	// check if instance can be declared closed
	int closed = instance_has_quorum(ii);
	if (!closed) {
		LOG(DBG, ("Not yet a quorum for iid:%u\n", aa->iid));
		return;
	}

	// Keep track of highest closed
	if (ii->iid > l->highest_iid_closed)
		l->highest_iid_closed = ii->iid;

	//If the closed instance is the current one,
	//Deliver it (and the followings if already closed)
	if (aa->iid == l->current_iid) {
		learner_deliver_next_closed(l);
	}
}

static void
learner_handle_msg(struct learner* l, struct bufferevent* bev)
{
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];

	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	evbuffer_remove(in, buffer, msg.data_size);
	
	switch (msg.type) {
        case accept_acks:
            learner_handle_accept_ack(l, (accept_ack*)buffer);
        	break;
        default:
            printf("Unknow msg type %d received from acceptors\n", msg.type);
    }
}

// TODO the following functions are basically duplicated in proposer.
static void
on_acceptor_msg(struct bufferevent* bev, void* arg)
{
	size_t len;
	paxos_msg msg;
	struct evbuffer* in;
	struct learner* l;
	
	l = arg;
	in = bufferevent_get_input(bev);
	
	while ((len = evbuffer_get_length(in)) > sizeof(paxos_msg)) {
		evbuffer_copyout(in, &msg, sizeof(paxos_msg));
		if (len < PAXOS_MSG_SIZE((&msg))) {
			LOG(DBG, ("not enough data\n"));
			return;
		}
		learner_handle_msg(l, bev);
	}
}

static void
on_event(struct bufferevent *bev, short events, void *ptr)
{
    if (events & BEV_EVENT_CONNECTED) {
    	LOG(VRB, ("Proposer connected...\n"));
	} else if (events & BEV_EVENT_ERROR) {
		LOG(VRB, ("Proposer connection error...\n"));
		bufferevent_disable(bev, EV_READ|EV_WRITE);
	}
}

static struct bufferevent* 
do_connect(struct learner* l, struct event_base* b, address* a)
{
	struct sockaddr_in sin;
	struct bufferevent* bev;
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(a->address_string);
	sin.sin_port = htons(a->port);
	
	bev = bufferevent_socket_new(b, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, on_acceptor_msg, NULL, on_event, l);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
	struct sockaddr* addr = (struct sockaddr*)&sin;
	if (bufferevent_socket_connect(bev, addr, sizeof(sin)) < 0) {
        bufferevent_free(bev);
        return NULL;
	}
	return bev;
}

/*-------------------------------------------------------------------------*/
// Public functions (see libpaxos.h for more details)
/*-------------------------------------------------------------------------*/

struct learner*
learner_init_conf(struct config* c, deliver_function f, void* arg,
struct event_base* b)
{
	int i;
	struct learner* l;
	
	l = malloc(sizeof(struct learner));
	l->conf = c;
	l->base = b;
	l->highest_iid_seen = 1; 
	l->current_iid = 1;
	l->highest_iid_closed = 0;
	l->delfun = f;
	l->delarg = arg;

	// check array size
	if ((LEARNER_ARRAY_SIZE & (LEARNER_ARRAY_SIZE -1)) != 0) {
		printf("Error: LEARNER_ARRAY_SIZE is not a power of 2\n");
		return NULL;
	}
	
	// clear the state array
	memset(l->instances, 0, (sizeof(struct instance) * LEARNER_ARRAY_SIZE));
	for (i = 0; i < LEARNER_ARRAY_SIZE; i++) {
		learner_clear_instance(l, i);
	}
	
	// setup connections to acceptors
	for (i = 0; i < l->conf->acceptors_count; i++) {
		l->acceptor_ev[i] = do_connect(l, b, &l->conf->acceptors[i]);
	}
	
    LOG(VRB, ("Learner is ready\n"));
    return l;
}

struct learner*
learner_init(const char* config_file, deliver_function f, void* arg, 
	struct event_base* b)
{
	struct config* c = read_config(config_file);
	if (c == NULL) return NULL;
	return learner_init_conf(c, f, arg, b);
}

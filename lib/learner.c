#include <stdlib.h>
#include <stdio.h>
#include <pthread.h> 
#include <assert.h>
#include <memory.h>

#include "event.h"
#include "evutil.h"

#include "libpaxos.h"
#include "libpaxos_priv.h"
#include "paxos_udp.h"

#define LEARNER_ERROR (-1)
#define LEARNER_READY (0)
#define LEARNER_STARTING (1)
#define INST_INFO_EMPTY (0)
#define IS_CLOSED(INST) (INST->final_value != NULL)

//Structure used to store all info relative to a given instance
typedef struct learner_instance_info {
    iid_t           iid;
    ballot_t        last_update_ballot;
    accept_ack*     acks[N_OF_ACCEPTORS];
    accept_ack*     final_value;
} l_inst_info;

//Highest instance for which a message was seen
static iid_t highest_iid_seen = 1;

//Current instance, incremented when current is closed and 
// the corresponding value is delivered
static iid_t current_iid = 1;

//Highest instance that is already closed 
// (can be higher than current!)
//TODO: not used
static iid_t highest_iid_closed = 0;

//Array (used as a circular buffer) to store instance infos
static l_inst_info learner_state[LEARNER_ARRAY_SIZE];

//A custom initialization function to invoke after the normal initialization
// Can be NULL
static custom_init_function custom_init = NULL;

//Function to invoke when the current_iid is closed, 
// the final value and some other informations is passed as argument
static deliver_function delfun = NULL;

//Current status of the learner and related signal
// The thread calling learner init waits until the new thread completed initialization
static int learner_ready = LEARNER_STARTING;
static pthread_mutex_t ready_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ready_cond = PTHREAD_COND_INITIALIZER;
static pthread_t learner_thread = NULL;
struct event init_complete_event;

//Timeval interval to schedule events as soon as possible
struct timeval asap_interval = {0, 0};

//Libevent handle
static struct event_base * eb;
// Event: a message was received
struct event learner_msg_event;
// Event: time to check for holes
static struct event hole_check_event;
//Time interval for the previous event
static struct timeval hole_check_interval;

//Network managers
static udp_send_buffer * to_acceptors;
static udp_receiver * for_learner;

/*-------------------------------------------------------------------------*/
// Helpers
/*-------------------------------------------------------------------------*/

//Used by the proposer to check for completion of phase 2
int learner_is_closed(iid_t iid) {
    l_inst_info * ii = GET_LEA_INSTANCE(iid);
    return ((iid == ii->iid) && IS_CLOSED(ii));
}

//Resets a given instance info
static void lea_clear_instance_info(l_inst_info * ii) {
    //Reset all fields and free stored messages
    ii->iid = INST_INFO_EMPTY;
    ii->last_update_ballot = 0;
    ii->final_value = NULL;
    size_t i;
    //Free all stored accept_ack
    for(i = 0; i < N_OF_ACCEPTORS; i++) {
        if(ii->acks[i] != NULL) {
            PAX_FREE(ii->acks[i]);
            ii->acks[i] = NULL;            
        }
    }
}

//Stores the accept_ack of a given acceptor in the corresponding record 
// at the appropriate index
// Assumes rec->acks[acc_id] is NULL already
static void lea_store_accept_ack(l_inst_info * ii, short int acceptor_id, accept_ack * aa) {
    accept_ack * new_ack = PAX_MALLOC(ACCEPT_ACK_SIZE(aa));
    memcpy(new_ack, aa, ACCEPT_ACK_SIZE(aa));
    //Store message at appropriate index (acceptor_id)
    ii->acks[acceptor_id] = new_ack;

    //Keep track of most recent accept_ack stored
    ii->last_update_ballot = aa->ballot;
}

//Tries to update the state based on the accept_ack received.
//Returns 0 if the message was discarded because not relevant. 1 if the state changed.
static int lea_update_state(l_inst_info * ii, short int acceptor_id, accept_ack * aa) {
    //First message for this iid
    if(ii->iid == INST_INFO_EMPTY) {
        LOG(DBG, ("Received first message for instance:%u\n", aa->iid));
        ii->iid = aa->iid;
        ii->last_update_ballot = aa->ballot;
    }
    assert(ii->iid == aa->iid);
    
    //Instance closed already, drop
    if(IS_CLOSED(ii)) {
        LOG(DBG, ("Dropping accept_ack for iid:%u, already closed\n", aa->iid));
        return 0;
    }
    
    //No previous message to overwrite for this acceptor
    if(ii->acks[acceptor_id] == NULL) {
        LOG(DBG, ("Got first ack for iid:%u, acceptor:%d\n", \
            ii->iid, acceptor_id));
        //Save this accept_ack
        lea_store_accept_ack(ii, acceptor_id, aa);
        ii->last_update_ballot = aa->ballot;
        return 1;
    }
    
    //There is already a message from the same acceptor
    accept_ack * prev_ack = ii->acks[acceptor_id];
    
    //Already more recent info in the record, accept_ack is old
    if(prev_ack->ballot >= aa->ballot) {
        LOG(DBG, ("Dropping accept_ack for iid:%u, stored ballot is newer or equal\n", aa->iid));
        return 0;
    }
    
    //Replace the previous ack since the received ballot is newer
    LOG(DBG, ("Overwriting previous accept_ack for iid:%u\n", aa->iid));
    PAX_FREE(prev_ack);
    lea_store_accept_ack(ii, acceptor_id, aa);
    ii->last_update_ballot = aa->ballot;
    return 1;
}

//Checks if a given instance is closed, that is if a quorum of acceptor
// accepted the same value+ballot
//Returns 1 if the instance is closed, 0 otherwise
static int lea_check_quorum(l_inst_info * ii) {
    size_t i, a_valid_index = -1, count = 0;
    accept_ack * curr_ack;
    
    //Iterates over stored acks
    for(i = 0; i < N_OF_ACCEPTORS; i++) {
        curr_ack = ii->acks[i];
        
        //No ack from this acceptor, skip
        if(curr_ack == NULL) {
            continue;
        }
        
        //Count the ones "agreeing" with the last added
        if(curr_ack->ballot == ii->last_update_ballot){
            a_valid_index = i;
            count++;
            
            //Special case: an acceptor is telling that
            //this value is -final-, it can be delivered 
            // immediately.
            if(curr_ack->is_final) {
                //For sure >= than quorum...
                count += N_OF_ACCEPTORS;
                break;
            }
        }
    }
    
    //Reached a quorum/majority!
    if(count >= QUORUM) {
        LOG(DBG, ("Reached quorum, iid:%u is closed!\n", ii->iid));
        ii->final_value = ii->acks[a_valid_index];
        
        //Keep track of highest closed
        if(ii->iid > highest_iid_closed) {
            highest_iid_closed = ii->iid;
        }

        return 1;
    }
    
    //No quorum yet...
    return 0;

}

//Invoked when the current_iid is closed.
// Since other instances may be closed too (curr+1, curr+2), also tries to deliver them
static void lea_deliver_next_closed() {
    //Get next instance (last delivered + 1)
    l_inst_info * ii = GET_LEA_INSTANCE(current_iid);
    accept_ack * aa;
    
    //If closed deliver it and all next closed
    while(IS_CLOSED(ii)) {
        assert(ii->iid == current_iid);
        aa = ii->final_value;
        
        //Deliver the value trough callback
        short int proposer_id = aa->ballot % MAX_N_OF_PROPOSERS;
        delfun(aa->value, aa->value_size, current_iid, aa->ballot, proposer_id);
        
        //Move to next instance
        current_iid++;
        
        //Clear the state
        lea_clear_instance_info(ii);
        
        //Go on and try to deliver next
        ii = GET_LEA_INSTANCE(current_iid);
    }

}

//Creates a batch of repeat_req to send to the acceptor, 
// asking to retransmit their accepted value for a given instance
static void 
lea_send_repeat_request(iid_t from, iid_t to) {
    iid_t i;
    
    l_inst_info * ii;
    //Create empty repeat_request in buffer
    sendbuf_clear(to_acceptors, repeat_reqs, -1);
    
    for(i = from; i < to; i++) {
        //Request all non-closed in from...to range
        ii = GET_LEA_INSTANCE(i);
        if(!IS_CLOSED(ii)) {
            sendbuf_add_repeat_req(to_acceptors, i);
        }
    }
    //Flush if dirty flag is set
    sendbuf_flush(to_acceptors);
}

//This function is invoked periodically and tries to detect if the learner 
// missed some message. For example if instance I is closed but instance I-1 
// it's not, we can't deliver I. So it will ask to the acceptors to repeat 
// their accepted value
static void
lea_hole_check(int fd, short event, void *arg) {
    UNUSED_ARG(fd);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);

    //Periodic check for missing instances
    //(i.e. i+1 closed, but i not closed yet)
    if (highest_iid_seen > current_iid + LEARNER_ARRAY_SIZE) {
        LOG(0, ("This learner is lagging behind!!!, highest seen:%u, highest delivered:%u\n", 
            highest_iid_seen, current_iid-1));
        lea_send_repeat_request(current_iid, highest_iid_seen);
    } else if(highest_iid_closed > current_iid) {
        LOG(VRB, ("Out of sync, highest closed:%u, highest delivered:%u\n", 
            highest_iid_closed, current_iid-1));
        //Ask retransmission to acceptors
        lea_send_repeat_request(current_iid, highest_iid_closed);
    }

    //Set the next timeout for calling this function
    if(event_add(&hole_check_event, &hole_check_interval) != 0) {
	   printf("Error while adding next hole_check event\n");
	}
}

/*-------------------------------------------------------------------------*/
// Event handlers
/*-------------------------------------------------------------------------*/

// Called when an accept_ack is received, the learner will update it's status
// for that instance and afterward check if the instance is closed
static void handle_accept_ack(short int acceptor_id, accept_ack * aa) {
    //Keep track of highest seen instance id
    if(aa->iid > highest_iid_seen) {
        highest_iid_seen = aa->iid;
    }
    
    //Already closed and delivered, ignore message
    if(aa->iid < current_iid) {
        LOG(DBG, ("Dropping accept_ack for already delivered iid:%u\n", aa->iid));
        return;
    }
    
    //We are late w.r.t the current iid, ignore message
    // (The instence received is too ahead and will overwrite something)
    if(aa->iid >= current_iid + LEARNER_ARRAY_SIZE) {
        LOG(DBG, ("Dropping accept_ack for iid:%u, too far in future\n", aa->iid));
        return;
    }

    //Message is within interesting bounds
    //Update the corresponding record
    l_inst_info * ii = GET_LEA_INSTANCE(aa->iid);
    int relevant = lea_update_state(ii, acceptor_id, aa);
    if(!relevant) {
        //Not really interesting (i.e. a duplicate message)
        LOG(DBG, ("Learner discarding learn for iid:%u\n", aa->iid));
        return;
    }
    
    // Message contained some relevant info, 
    // check if instance can be declared closed
    int closed = lea_check_quorum(ii);
    if(!closed) {
        LOG(DBG, ("Not yet a quorum for iid:%u\n", aa->iid));
        return;
    }

    //If the closed instance is the current one,
    //Deliver it (and the followings if already closed)
    if (aa->iid == current_iid) {
        lea_deliver_next_closed(aa->iid);
    }
}

// Called when an accept_ack_batch is received
static void handle_accept_ack_batch(accept_ack_batch* aab) {
    size_t data_offset;
    accept_ack * aa;
    
    data_offset = 0;

    short int i;
    //Iterate over accept_ack messages in batch
    for(i = 0; i < aab->count; i++) {
        aa = (accept_ack*) &aab->data[data_offset];
        handle_accept_ack(aab->acceptor_id, aa);
        data_offset += ACCEPT_ACK_SIZE(aa);
    }    
}

// Invoked by libevent when a new message was received
static void lea_handle_newmsg(int sock, short event, void *arg) {
    //Make the compiler happy!
    UNUSED_ARG(sock);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    
    assert(sock == for_learner->sock);

    //Read and validate next message from socket
    int valid = udp_read_next_message(for_learner);    
    if (valid < 0) {
        printf("Dropping invalid learner message\n");
        return;
    }
    
    paxos_msg * msg = (paxos_msg*) &for_learner->recv_buffer;
    switch(msg->type) {
        case accept_acks: {
            handle_accept_ack_batch((accept_ack_batch*) msg->data);
        }
        break;

        default: {
            printf("Unknow msg type %d received by learner\n", msg->type);
        }
    }
}

/*-------------------------------------------------------------------------*/
// Initialization
/*-------------------------------------------------------------------------*/

//Initialize records array (circular buffer)
static int init_lea_structs() {
    // Check array size
    if ((LEARNER_ARRAY_SIZE & (LEARNER_ARRAY_SIZE -1)) != 0) {
        printf("Error: LEARNER_ARRAY_SIZE is not a power of 2\n");
        return LEARNER_ERROR;
    }
    
    // Clear the state array
    memset(learner_state, 0, (sizeof(l_inst_info) * LEARNER_ARRAY_SIZE));
    size_t i;
    for(i = 0; i < LEARNER_ARRAY_SIZE; i++) {
        lea_clear_instance_info(&learner_state[i]);
    }
    return 0;
}

//Initializes socket managers and relative events
static int init_lea_network() {
    
    // Send buffer for talking to acceptors
    to_acceptors = udp_sendbuf_new(PAXOS_ACCEPTORS_NET);
    if(to_acceptors == NULL) {
        printf("Error creating learner network sender\n");
        return LEARNER_ERROR;
    }
    
    // Message receive event
    for_learner = udp_receiver_new(PAXOS_LEARNERS_NET);
    if (for_learner == NULL) {
        printf("Error creating learner network receiver\n");
        return LEARNER_ERROR;
    }
    event_set(&learner_msg_event, for_learner->sock, EV_READ|EV_PERSIST, lea_handle_newmsg, NULL);
    event_add(&learner_msg_event, NULL);
    
    return 0;
}

//Set up the first timer for hole checking
static int 
init_lea_timers() {
    evtimer_set(&hole_check_event, lea_hole_check, NULL);
	evutil_timerclear(&hole_check_interval);
	hole_check_interval.tv_sec = LEARNER_HOLECHECK_INTERVAL / 1000000;
    hole_check_interval.tv_usec = LEARNER_HOLECHECK_INTERVAL % 1000000;
	if(event_add(&hole_check_event, &hole_check_interval) != 0) {
	   printf("Error while adding first periodic hole_check event\n");
       return -1;
	}
    
    return 0;
}

//Invoked when learner init fails. Sets the state to error and 
// wakes up the thread that called learner_init
static void 
init_lea_failure(char * msg) {
    //Init failed for some reason
    printf("Learner init error: %s\n", msg);
    
    //Set status to error and wake up 
    //the thread that called learner_init
    pthread_mutex_lock(&ready_lock);
    learner_ready = LEARNER_ERROR;
    pthread_cond_signal(&ready_cond);
    pthread_mutex_unlock(&ready_lock);
}

//Invoked as first event of libevent loop, if learner init 
// completes successfully. Sets the state to ready
//  and wakes up the thread that called learner_init
static void
init_lea_success(int fd, short event, void *arg) {
    UNUSED_ARG(fd);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);

    LOG(DBG, ("Learner thread setting status to ready\n"));
    //Init completed successfully, wake up
    //the thread that called learner_init
    pthread_mutex_lock(&ready_lock);
    learner_ready = LEARNER_READY;
    pthread_cond_signal(&ready_cond);
    pthread_mutex_unlock(&ready_lock);
}

//Set up the first event to be executed ASAP, 
// signaling that the initialization is complete
static int
init_lea_signal_ready() {
    evtimer_set(&init_complete_event, init_lea_success, NULL);
	if(event_add(&init_complete_event, &asap_interval) != 0) {
	   printf("Error while adding lea successful init event\n");
       return -1;
   }
   return 0;
}


//This function is invoked by libevent in a new thread. It initializes the learner, 
// and starts the libevent loop (which never returns)
static void* 
init_learner_thread(void* arg) {
    //The deliver callback cannot be null
    //(why starting a learner otherwise?)
    delfun = (deliver_function) arg;
    if(delfun == NULL) {
        init_lea_failure("Error in libevent init\n");
        printf("Error NULL callback!\n");
        return NULL;
    }
    
    //Initialization of libevent handle
    if((eb = event_init()) == NULL) {
        init_lea_failure("Error in libevent init\n");
        return NULL;
    }
    
    //Normal learner initialization, private structures
    if(init_lea_structs() != 0) {
        init_lea_failure("Error in learner structures initialization\n");
        return NULL;
    }
    
    //Init sockets and send buffer
    if(init_lea_network() != 0) {
        init_lea_failure("Error in learner network init\n");
        return NULL;
    }

    //Timers initialization
    if(init_lea_timers() != 0) {
        init_lea_failure("Error in learner timers initialization\n");
        return NULL;
    }
    
    //Call custom init (i.e. to register additional events)
    if(custom_init != NULL && custom_init() != 0) {
        init_lea_failure("Error in custom_init_function\n");
        return NULL;
    } else {
        LOG(DBG, ("Custom init completed\n"));
    }
    
    // Signal client, learner is ready
    if(init_lea_signal_ready() != 0) {
        init_lea_failure("Error in learner timers initialization\n");
        return NULL;
    }

    // Start the libevent loop, should never return
    LOG(DBG, ("Learner thread ready, starting libevent loop\n"));
    event_dispatch();
    printf("libeven loop terminated\n");
    return NULL;
}

// This function waits until the learner status is set 
// (to 'ready' or to 'error') and returns the corresponding value
static int 
init_lea_wait_ready() {
    int status;
    
    pthread_mutex_lock(&ready_lock);
    
    while(1) {
        //Wait for a signal
        pthread_cond_wait(&ready_cond, &ready_lock);
        status = learner_ready;

        if(status == LEARNER_STARTING) {
        //Not ready yet, keep waiting
            continue;            
        } else {
        //Status changed
            break;
        }
    }
    
    //Check that status is a valid value
    if (status != LEARNER_READY && status != LEARNER_ERROR) {
        printf("Unknow learner status: %d\n", status);
        status = LEARNER_ERROR;
    }
    
    pthread_mutex_unlock(&ready_lock);
    return status;
}

/*-------------------------------------------------------------------------*/
// Public functions (see libpaxos.h for more details)
/*-------------------------------------------------------------------------*/

int learner_init(deliver_function f, custom_init_function cif) {
    // Start learner (which starts event_dispatch())
    custom_init = cif;
    if (pthread_create(&learner_thread, NULL, init_learner_thread, (void*) f) != 0) {
        perror("pthread create learner thread");
        return -1;
    }
    
    //Wait until initialization completed
    LOG(DBG, ("Learner thread started, waiting for ready signal\n"));    
    if (init_lea_wait_ready() == LEARNER_ERROR) {
        printf("Learner initialization failed!\n");
        return -1;
    }
    
    LOG(VRB, ("Learner is ready\n"));    
    return 0;
}

//TODO: comment or categorize...
void learner_suspend() {
    //Remove active events
    event_del(&learner_msg_event);
    //Close socket
    udp_receiver_destroy(for_learner);
    for_learner = NULL;
        
    LOG(VRB, ("Learner events suspended!\n"));
}

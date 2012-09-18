#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "event.h"
#include "libpaxos.h"
#include "libpaxos_priv.h"
#include "paxos_udp.h"

static int vh_list_size = 0;
static vh_value_wrapper * vh_list_head = NULL;
static vh_value_wrapper * vh_list_tail = NULL;

static long unsigned int dropped_count = 0;

static struct event leader_msg_event;
static udp_receiver * for_leader;

static pthread_mutex_t pending_list_lock = PTHREAD_MUTEX_INITIALIZER;

vh_value_wrapper * 
vh_wrap_value(char * value, size_t size) {
    vh_value_wrapper * vw = PAX_MALLOC(sizeof(vh_value_wrapper) + size);
    vw->value_size = size;
    vw->next = NULL;
    //Copy value in
    memcpy(vw->value, value, size);
    return vw;
}

//Return 0 for equals, like memcmp()
int vh_value_compare(vh_value_wrapper * vw1, vh_value_wrapper * vw2) {
    if(vw1->value_size != vw2->value_size) {
        return -1;
    }
    return memcmp(vw1->value, vw2->value, vw1->value_size);
}

static void 
vh_handle_newmsg(int sock, short event, void *arg) {
    //Make the compiler happy!
    UNUSED_ARG(sock);
    UNUSED_ARG(event);
    UNUSED_ARG(arg);
    
    assert(sock == for_leader->sock);
    
    //Read the next message
    int valid = udp_read_next_message(for_leader);
    if (valid < 0) {
        printf("Dropping invalid client-to-leader message\n");
        return;
    }

    //The message is valid, take the appropriate action
    // based on the type
    paxos_msg * msg = (paxos_msg*) &for_leader->recv_buffer;
    switch(msg->type) {
        case submit: {
            vh_enqueue_value(msg->data, msg->data_size);
        }
        break;

        default: {
            printf("Unknow msg type %d received by proposer\n", msg->type);
        }
    }
}

int
vh_init() {
    //Create the emtpy values list
    vh_list_size = 0;
    vh_list_head = NULL;
    vh_list_tail = NULL;
    dropped_count = 0;
    
    // Start listening on net where clients send values
    for_leader = udp_receiver_new(PAXOS_SUBMIT_NET);
    if (for_leader == NULL) {
        printf("Error creating proposer network receiver\n");
        return -1;
    }
    event_set(&leader_msg_event, for_leader->sock, EV_READ|EV_PERSIST, vh_handle_newmsg, NULL);
    event_add(&leader_msg_event, NULL);    
    
    return 0;
}

void 
vh_shutdown() {
    //Delete event
    event_del(&leader_msg_event);

    //Close socket and free receiver
    udp_receiver_destroy(for_leader);
    
    //All values in pending could not be delivered yet.
    // Notify the respective clients
    vh_value_wrapper * vw;
    while ((vw = vh_get_next_pending()) != NULL) {
        vh_notify_client(-1, vw);
        PAX_FREE(vw);        
    }
}


int vh_pending_list_size() {
    pthread_mutex_lock(&pending_list_lock);
    int ls = vh_list_size;
    pthread_mutex_unlock(&pending_list_lock);
    return ls;
}

long unsigned int vh_get_dropped_count() {
    pthread_mutex_lock(&pending_list_lock);
    int dc = dropped_count;
    pthread_mutex_unlock(&pending_list_lock);
    return dc;
}

void vh_enqueue_value(char * value, size_t value_size) {
    
    pthread_mutex_lock(&pending_list_lock);
    //Create wrapper
    
    if(vh_list_size > LEADER_MAX_QUEUE_LENGTH) {
        pthread_mutex_unlock(&pending_list_lock);
        LOG(VRB, ("Value dropped, list is already too long\n"));
#ifdef LEADER_EVENTS_UPDATE_INTERVAL
        dropped_count += 1;
#endif
        return;
    }
    
    vh_value_wrapper * new_vw = vh_wrap_value(value, value_size);
    
    /* List is empty*/
	if (vh_list_head == NULL && vh_list_tail == NULL) {
		vh_list_head = new_vw;
		vh_list_tail = new_vw;
        vh_list_size = 1;

	/* List is not empty*/
	} else {
		vh_list_tail->next = new_vw;
		vh_list_tail = new_vw;
        vh_list_size += 1;
	}
    pthread_mutex_unlock(&pending_list_lock);
    LOG(DBG, ("Value of size %lu enqueued\n", value_size));
}

vh_value_wrapper * 
vh_get_next_pending() {
    pthread_mutex_lock(&pending_list_lock);
    
    /* List is empty*/
	if (vh_list_head == NULL && vh_list_tail == NULL) {
        pthread_mutex_unlock(&pending_list_lock);
        return NULL;
    }
    
    /* Pop */
    vh_value_wrapper * first_vw = vh_list_head;
    vh_list_head = first_vw->next;

    /* Also last element */
    if(vh_list_tail == first_vw) {
        vh_list_tail = NULL;
    }
    vh_list_size -= 1;
    pthread_mutex_unlock(&pending_list_lock);

    LOG(DBG, ("Popping value of size %lu\n", first_vw->value_size));
    return first_vw;
}

void 
vh_push_back_value(vh_value_wrapper * vw) {
    pthread_mutex_lock(&pending_list_lock);

    /* Adds as list head*/
    vw->next = vh_list_head;
    
    /* List is empty*/
	if (vh_list_head == NULL && vh_list_tail == NULL) {
		vh_list_head = vw;
		vh_list_tail = vw;
        vh_list_size = 1;

	/* List is not empty*/
	} else {
		vh_list_head = vw;
        vh_list_size += 1;
	}
    pthread_mutex_unlock(&pending_list_lock);
}

void vh_notify_client(unsigned int result, vh_value_wrapper * vw) {
    // This is a stub for notifying a client that its value
    // could not be delivered (notice that the value may actually
    // be delivered afterward by some other proposer)
    if(result != 0) {
        LOG(DBG, ("Notify client -> Submit failed\n"));
    } else {
        LOG(DBG, ("Notify client -> Submit successful\n"));
    }
}

void pax_submit_sharedmem(char* value, size_t val_size) {
    vh_enqueue_value(value, val_size);
}

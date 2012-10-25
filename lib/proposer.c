#include "libpaxos.h"
#include "config_reader.h"
#include "carray.h"
#include "tcp_receiver.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>


struct phase1_info
{
	unsigned int    pending_count;
	unsigned int    ready_count;
	iid_t           highest_open;
};

struct phase2_info
{
    iid_t next_unused_iid;
    unsigned int open_count;
};

typedef enum instance_status_e
{
    empty, 
    p1_pending,
    p1_ready,
    p2_pending,
    p2_completed
} i_status;

struct instance
{
	iid_t 			iid;
	i_status     	status;
	ballot_t        my_ballot;
	ballot_t        p1_value_ballot;
	unsigned int    promises_count;
	unsigned int    promises_bitvector;
	paxos_msg*		p1_value;
	paxos_msg*		p2_value;
};

struct proposer
{
	int id;
	iid_t current_iid;	// Lowest instance for which no value has been chosen
	int acceptors_count;
	struct instance instances[PROPOSER_ARRAY_SIZE];
	struct bufferevent* acceptor_ev[N_OF_ACCEPTORS];
	struct phase1_info p1_info;
	struct phase2_info p2_info;
	struct tcp_receiver* receiver;
	struct learner* l;
	struct carray* client_values;
	struct event_base* base;
};


struct learner* 
learner_init_conf(config* c, deliver_function f, void* arg, 
	struct event_base* b);


static ballot_t 
proposer_next_ballot(struct proposer* p, ballot_t b)
{
	if (b > 0)
		return MAX_N_OF_PROPOSERS + p->id;
	else
		return MAX_N_OF_PROPOSERS + b;
}

static struct instance*
proposer_get_instance(struct proposer* p, iid_t iid)
{
	return &(p->instances[(iid & (PROPOSER_ARRAY_SIZE-1))]);
}

static void
proposer_clear_instance(struct proposer* p, iid_t iid)
{
	struct instance * ii;
	ii = proposer_get_instance(p, iid);
	ii->iid = 0;
    ii->status = empty;
    ii->my_ballot = 0;
    ii->p1_value_ballot = 0;
    ii->promises_bitvector = 0;
    ii->promises_count = 0;
    if (ii->p1_value != NULL) free(ii->p1_value);
    if (ii->p2_value != NULL) free(ii->p2_value);
    ii->p1_value = NULL;
    ii->p2_value = NULL;
}

static void
do_phase_1(struct proposer* p)
{
	int i;
	struct instance* ii;
	iid_t iid = p->p1_info.highest_open + 1;
	
	// Get instance from state array
	ii = proposer_get_instance(p, iid);

	if (ii->status == empty) {
		ii->iid = iid;
	    ii->status = p1_pending;
	    ii->my_ballot = proposer_next_ballot(p, ii->my_ballot);
	} else if (ii->status == p1_pending) {
		assert(ii->iid == iid);
		//Reset fields used for previous phase 1
		ii->promises_bitvector = 0;
		ii->promises_count = 0;
		ii->p1_value_ballot = 0;
		if (ii->p1_value != NULL) free(ii->p1_value);
		ii->p1_value = NULL;
		//Ballot is incremented
		ii->my_ballot = proposer_next_ballot(p, ii->my_ballot);
	}
	
	for (i = 0; i < p->acceptors_count; i++) {
		sendbuf_add_prepare_req(p->acceptor_ev[i],
			ii->iid, ii->my_ballot);
	}
	
	p->p1_info.pending_count++;
	p->p1_info.highest_open++;
}

static void
proposer_open_phase_1(struct proposer* p)
{
	int i, active;

	active = p->p1_info.pending_count + p->p1_info.ready_count;
	if (active >= PROPOSER_PREEXEC_WIN_SIZE) {
        return;
    }

	int to_open = PROPOSER_PREEXEC_WIN_SIZE - active;
	for (i = 0; i < to_open; i++) {
		do_phase_1(p);
	}
	
	LOG(DBG, ("Opened %d new instances\n", to_open));
}

static int
value_cmp(paxos_msg* m1, paxos_msg* m2)
{
	assert(m1->type == m2->type);
	if (m1->data_size != m2->data_size)
		return -1;
	return memcmp(m1->data, m2->data, m1->data_size);
}

static void
do_phase_2(struct proposer* p)
{
	int i;
	struct instance* ii;
	iid_t iid = p->p2_info.next_unused_iid;
    
	ii = proposer_get_instance(p, iid);
	
    if (ii->p1_value == NULL && ii->p2_value == NULL) {
        //Happens when p1 completes without value        
        //Assign a p2_value and execute
        ii->p2_value = carray_pop_front(p->client_values);
        assert(ii->p2_value != NULL);
    } else if (ii->p1_value != NULL) {
        //Only p1 value is present, MUST execute p2 with it
        //Save it as p2 value and execute
        ii->p2_value = ii->p1_value;
        ii->p1_value = NULL;
        ii->p1_value_ballot = 0;
    } else if (ii->p2_value != NULL) {
        // Only p2 value is present
        // Do phase 2 with it
    } else {
        // There are both p1 and p2 value
		// Compare them
        if (value_cmp(ii->p1_value, ii->p2_value) == 0) {
            // Same value, just delete p1_value
            free(ii->p1_value);
            ii->p1_value = NULL;
            ii->p1_value_ballot = 0;
        } else {
            // Different values
            // p2_value is pushed back to pending list
			carray_push_back(p->client_values, ii->p2_value);
			// Must execute p2 with p1 value
            ii->p2_value = ii->p1_value;
            ii->p1_value = NULL;
            ii->p1_value_ballot = 0;            
        }
    }
    //Change instance status
    ii->status = p2_pending;

    //Send the accept request
	LOG(DBG, ("sending accept req for instance %d ballot %d\n", ii->iid, ii->my_ballot));
	for (i = 0; i < N_OF_ACCEPTORS; i++) {
    	sendbuf_add_accept_req(p->acceptor_ev[i], ii->iid,
			ii->my_ballot, ii->p2_value);
	}
	
	p->p2_info.next_unused_iid += 1;
}

static void
proposer_open_phase_2(struct proposer* p)
{
	struct instance* ii;
	while (!carray_empty(p->client_values)) {
		ii = proposer_get_instance(p, p->p2_info.next_unused_iid);
		if (ii->status != p1_ready || 
			ii->iid != p->p2_info.next_unused_iid) {
            LOG(DBG, ("Next instance to use for P2 (iid:%u) is not ready yet\n", p->p2_info.next_unused_iid));
            break;
        }
		// We do both phase 1 and 2
		do_phase_1(p);
		do_phase_2(p);
	}
}

static paxos_msg*
wrap_value(char* value, size_t size)
{
	paxos_msg* msg = malloc(size + sizeof(paxos_msg));
	msg->data_size = size;
	msg->type = submit;
	memcpy(msg->data, value, size);
	return msg;
}

static void
proposer_save_prepare_ack(struct instance* ii, prepare_ack* pa)
{
    if (ii->promises_bitvector & (1<<pa->acceptor_id)) {
        LOG(DBG, ("Dropping duplicate promise from:%d, iid:%u, \n", pa->acceptor_id, ii->iid));
        return;
    }
    
    ii->promises_bitvector &= (1<<pa->acceptor_id);
    ii->promises_count++;
    LOG(DBG, ("Received valid promise from: %d, iid: %u, \n", pa->acceptor_id, ii->iid));
    
    if (pa->value_size == 0) {
        LOG(DBG, ("No value in promise\n"));
        return;
    }

    // Our value has same or greater ballot
    if (ii->p1_value_ballot >= pa->value_ballot) {
        // Keep the current value
        LOG(DBG, ("Included value is ignored (cause:value_ballot)\n"));
        return;
    }
    
    // Ballot is greater but the value is actually the same
    if ((ii->p1_value != NULL) &&
        (ii->p1_value->data_size == pa->value_size) && 
        (memcmp(ii->p1_value->data, pa->value, pa->value_size) == 0)) {
        //Just update the value ballot
        LOG(DBG, ("Included value is the same with higher value_ballot\n"));
        ii->p1_value_ballot = pa->value_ballot;
        return;
    }
    
    //Value should replace the one we have (if any)
    if (ii->p1_value != NULL) {
        free(ii->p1_value);
    }
    
    //Save the received value 
    ii->p1_value = wrap_value(pa->value, pa->value_size);
    ii->p1_value_ballot = pa->value_ballot;
    LOG(DBG, ("Value in promise saved\n"));
}

static void
proposer_handle_prepare_ack(struct proposer* p, prepare_ack* pa)
{
	struct instance* ii;
	
	ii = proposer_get_instance(p, pa->iid);
	    
    // If not p1_pending, drop
    if (ii->status != p1_pending) {
        LOG(DBG, ("Promise dropped, iid:%u not pending\n", pa->iid));
        return;
    }
    
    // If not our ballot, drop
    if (pa->ballot != ii->my_ballot) {
        LOG(DBG, ("Promise dropped, iid:%u not our ballot\n", pa->iid));
        return;
    }
    
    //Save the acknowledgement from this acceptor
    //Takes also care of value that may be there
    proposer_save_prepare_ack(ii, pa);
    
    //Not a majority yet for this instance
    if (ii->promises_count < QUORUM) {
        LOG(DBG, ("Not yet a quorum for iid:%u\n", pa->iid));
        return;
    }
    
    //Quorum reached!
	ii->status = p1_ready;
	p->p1_info.pending_count -= 1;
	p->p1_info.ready_count += 1;

    LOG(DBG, ("Quorum for iid:%u reached\n", pa->iid));

    //Some instance completed phase 1
    // if (ready > 0) {
    //     // Send a value for p2 timed-out that 
    //     // had to go trough phase 1 again
    //     leader_open_instances_p2_expired();
    //     // try to send a value in phase 2
    //     // for new instances
    //     leader_open_instances_p2_new();
    // }
}

static void
proposer_handle_msg(struct proposer* p, struct bufferevent* bev)
{
	paxos_msg msg;
	struct evbuffer* in;
	char* buffer[PAXOS_MAX_VALUE_SIZE];

	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	evbuffer_remove(in, buffer, msg.data_size);
	
	switch (msg.type) {
        case prepare_acks:
            proposer_handle_prepare_ack(p, (prepare_ack*)buffer);
			proposer_open_phase_2(p);
        	break;
        default:
			LOG(VRB, ("Unknown msg type %d received from acceptors\n", msg.type));
    }
}

static void
on_acceptor_msg(struct bufferevent* bev, void* arg)
{
	size_t len;
	paxos_msg msg;
	struct evbuffer* in;
	struct proposer* p;
	
	p = arg;
	in = bufferevent_get_input(bev);
	
	while ((len = evbuffer_get_length(in)) > sizeof(paxos_msg)) {
		evbuffer_copyout(in, &msg, sizeof(paxos_msg));
		if (len < PAXOS_MSG_SIZE((&msg))) {
			LOG(DBG, ("not enough data\n"));
			return;
		}
		proposer_handle_msg(p, bev);
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
do_connect(struct proposer* p, struct event_base* b, address* a) 
{
	struct sockaddr_in sin;
	struct bufferevent* bev;
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(a->address_string);
	sin.sin_port = htons(a->port);
	
	bev = bufferevent_socket_new(b, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, on_acceptor_msg, NULL, on_event, p);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
	struct sockaddr* addr = (struct sockaddr*)&sin;
	if (bufferevent_socket_connect(bev, addr, sizeof(sin)) < 0) {
        bufferevent_free(bev);
        return NULL;
	}
	return bev;
}

static void
on_client_msg(struct bufferevent* bev, void* arg)
{
	char* val;
	paxos_msg msg;
	struct evbuffer* in;
	struct proposer* p = arg;
	
	in = bufferevent_get_input(bev);
	evbuffer_copyout(in, &msg, sizeof(paxos_msg));
	
    switch (msg.type) {
        case submit:
			val = malloc(PAXOS_MSG_SIZE((&msg)));
			evbuffer_remove(in, val, PAXOS_MSG_SIZE((&msg)));
			carray_push_back(p->client_values, val);
			proposer_open_phase_2(p);
        	break;
        default:
            printf("Unknown msg type %d received from client\n", msg.type);
    }
}

static void 
proposer_learn(char * value, size_t size, iid_t iid, ballot_t b, 
	int proposer, void* arg)
{
	struct proposer* p;
	struct instance* ii;
	
    LOG(DBG, ("Instance %u delivered to Leader\n", iid));
	
	p = arg;
	ii = proposer_get_instance(p, iid);
	
    if (ii->iid != iid) {
	    //Instance not even initialized, skip
        return;
    }
    
    if (ii->status == p1_pending) {
        p->p1_info.pending_count -= 1;
    }
    
    if (p->p2_info.next_unused_iid == iid) {
		p->p2_info.next_unused_iid += 1;
    }
    
    int opened_by_me = (ii->status == p1_pending && ii->p2_value != NULL) ||
        (ii->status == p1_ready && ii->p2_value != NULL) ||
        (ii->status == p2_pending);

    if (opened_by_me) {
		p->p2_info.open_count -= 1;
    }

    int my_val = (ii->p2_value != NULL) &&
        (ii->p2_value->data_size == size) &&
        (memcmp(value, ii->p2_value->data, size) == 0);

    if (my_val) {
		//Our value accepted, notify client that submitted it
        // vh_notify_client(0, ii->p2_value); //TODO what the hell is that??
    } else if (ii->p2_value != NULL) {
		//Different value accepted, push back our value
		carray_push_back(p->client_values, ii->p2_value);
        ii->p2_value = NULL;
    } else {
        //We assigned no value to this instance,
        //it comes from somebody else??
    }

    // Clear current instance
	proposer_clear_instance(p, iid);
    
    // If enough instances are ready to 
    // be opened, start phase2 for them
    // leader_open_instances_p2_new();
}

struct proposer*
proposer_init(int id, const char* config_file, struct event_base* b)
{
	int i;
	struct proposer* p;
	
    config* conf = read_config(config_file);
	if (conf == NULL)
		return NULL;
	
    // Check id validity of proposer_id
    if (id < 0 || id >= MAX_N_OF_PROPOSERS) {
        printf("Invalid proposer id:%d\n", id);
        return NULL;
    }

	p = malloc(sizeof(struct proposer));

	p->id = id;
	p->base = b;
	p->current_iid = 1;
	p->acceptors_count = conf->acceptors_count;
		
	// Reset phase 1 counters
	p->p1_info.pending_count = 0;
	p->p1_info.ready_count = 0;
    // Set so that next p1 to open is current_iid
    p->p1_info.highest_open = p->current_iid - 1;

    // Reset phase 2 counters
    p->p2_info.next_unused_iid = p->current_iid;
    p->p2_info.open_count = 0;

    LOG(VRB, ("Proposer %d starting...\n", id));
	
	// clear all instances
	memset(p->instances, 0, (sizeof(struct instance) * PROPOSER_ARRAY_SIZE));
	for (i = 0; i < PROPOSER_ARRAY_SIZE; i++) {
        proposer_clear_instance(p, i);
    }
	
	// Setup client listener
	p->client_values = carray_new(2000);
 	p->receiver = tcp_receiver_new(b, &conf->proposers[id], on_client_msg, p);
	
	// Setup connections to acceptors
	for (i = 0; i < conf->acceptors_count; i++) {
		p->acceptor_ev[i] = do_connect(p, b, &conf->acceptors[i]);
		assert(p->acceptor_ev[i] != NULL);
	}
	
	// Setup the learner
	p->l = learner_init_conf(conf, proposer_learn, p, p->base);
	if (p->l == NULL) {
		printf("learner_init failed\n");
		return NULL;
	}
	
	proposer_open_phase_1(p);
	
    LOG(VRB, ("Proposer is ready\n"));
    return p;
}

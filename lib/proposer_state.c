#include "proposer_state.h"
#include "learner_state.h"
#include "carray.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

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
	int				id;
	iid_t 			iid;
	i_status     	status;
	ballot_t        my_ballot;
	ballot_t        p1_value_ballot;
	unsigned int    promises_count;
	unsigned int    promises_bitvector;
	paxos_msg*		p1_value;
	paxos_msg*		p2_value;
};


struct proposer_state 
{
	int id;
	iid_t next_prepare_iid;
    iid_t next_accept_iid;
	struct carray* values;
	struct carray* instances;
	struct learner_state* learner;
};


static int
value_cmp(paxos_msg* m1, paxos_msg* m2)
{
	assert(m1->type == m2->type);
	if (m1->data_size != m2->data_size)
		return -1;
	return memcmp(m1->data, m2->data, m1->data_size);
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
instance_clear(struct instance* inst)
{
	if (inst->p1_value != NULL) free(inst->p1_value);
    if (inst->p2_value != NULL) free(inst->p2_value);
	memset(inst, 0, sizeof(struct instance));
    inst->status = empty;
}

static struct instance*
instance_new()
{
	struct instance* inst;
	inst = malloc(sizeof(struct instance));
	instance_clear(inst);
	return inst;
}

static void
instance_add_prepare_ack(struct instance* inst, prepare_ack* ack)
{
    if (inst->promises_bitvector & (1<<ack->acceptor_id)) {
        LOG(DBG, ("Dropping duplicate promise from:%d, iid:%u, \n", 
			ack->acceptor_id, inst->iid));
        return;
    }
    
    inst->promises_bitvector &= (1<<ack->acceptor_id);
    inst->promises_count++;
    LOG(DBG, ("Received valid promise from: %d, iid: %u, \n",
		ack->acceptor_id, inst->iid));
    
    if (ack->value_size == 0) {
        LOG(DBG, ("No value in promise\n"));
        return;
    }

    // Our value has same or greater ballot
    if (inst->p1_value_ballot >= ack->value_ballot) {
        // Keep the current value
        LOG(DBG, ("Included value is ignored (cause:value_ballot)\n"));
        return;
    }
    
    // Ballot is greater but the value is actually the same
    if ((inst->p1_value != NULL) &&
        (inst->p1_value->data_size == ack->value_size) && 
        (memcmp(inst->p1_value->data, ack->value, ack->value_size) == 0)) {
        //Just update the value ballot
        LOG(DBG, ("Included value is the same with higher value_ballot\n"));
        inst->p1_value_ballot = ack->value_ballot;
        return;
    }
    
    // Value should replace the one we have (if any)
    if (inst->p1_value != NULL) {
        free(inst->p1_value);
    }
    
    // Save the received value 
    inst->p1_value = wrap_value(ack->value, ack->value_size);
    inst->p1_value_ballot = ack->value_ballot;
    LOG(DBG, ("Value in promise saved\n"));
}

static struct instance*
proposer_state_get_instance(struct proposer_state* s, iid_t iid)
{
	struct instance* inst;
	inst = carray_at(s->instances, iid);
	assert(inst->iid == iid || inst->iid == 0);
	return inst;
}

static int
proposer_state_instance_ready(struct proposer_state* s, iid_t iid)
{
	struct instance* inst;
	inst = carray_at(s->instances, iid);
	if (inst->status != p1_ready || inst->iid != iid) {
		return 0;
	}
	return 1;
}

static ballot_t 
proposer_state_next_ballot(struct proposer_state* s, ballot_t b)
{
	if (b > 0)
		return MAX_N_OF_PROPOSERS + s->id;
	else
		return MAX_N_OF_PROPOSERS + b;
}

void
proposer_state_propose(struct proposer_state* s, paxos_msg* msg)
{
	carray_push_back(s->values, msg);
}

void 
proposer_state_prepare(struct proposer_state* s, iid_t* i, ballot_t* b)
{
	struct instance* inst;
	iid_t iid = s->next_prepare_iid + 1;
	
	// Get instance from state array
	inst = proposer_state_get_instance(s, iid);

	if (inst->status == empty) {
		inst->iid = iid;
	   	inst->status = p1_pending;
	    inst->my_ballot = proposer_state_next_ballot(s, inst->my_ballot);
	} else if (inst->status == p1_pending) {
		assert(inst->iid == iid);
		//Reset fields used for previous phase 1
		inst->promises_bitvector = 0;
		inst->promises_count = 0;
		inst->p1_value_ballot = 0;
		if (inst->p1_value != NULL) free(inst->p1_value);
		inst->p1_value = NULL;
		//Ballot is incremented
		inst->my_ballot = proposer_state_next_ballot(s, inst->my_ballot);
	}
	
	s->next_prepare_iid++;

	// return values
	*i = inst->iid;
	*b = inst->my_ballot;
}

void
proposer_state_receive_prepare(struct proposer_state* s, prepare_ack* ack)
{
	struct instance* inst;
	inst = proposer_state_get_instance(s, ack->iid);
	
    // If not p1_pending, drop
    if (inst->status != p1_pending) {
        LOG(DBG, ("Promise dropped, iid:%u not pending\n", ack->iid));
        return;
    }

    // Save the acknowledgement from this acceptor.
    // Also takes care of value that may be there.
    instance_add_prepare_ack(inst, ack);
	
	// Not a majority yet for this instance
    if (inst->promises_count < QUORUM) {
        LOG(DBG, ("Not yet a quorum for iid:%u\n", ack->iid));
        return;
    }
	
    // Quorum reached!
	inst->status = p1_ready;

    LOG(DBG, ("Quorum for iid:%u reached\n", ack->iid));
}

int 
proposer_state_accept(struct proposer_state* s, iid_t* iout,
	ballot_t* bout, paxos_msg** vout)
{
	struct instance* inst;
	iid_t iid = s->next_accept_iid + 1;
	
	if (!proposer_state_instance_ready(s, iid) || carray_empty(s->values))
		return 0;
	
	inst = proposer_state_get_instance(s, iid);
		
    if (inst->p1_value == NULL && inst->p2_value == NULL) {
		// Happens when p1 completes without value        
		// Assign a p2_value and execute
		inst->p2_value = carray_pop_front(s->values);
        assert(inst->p2_value != NULL);
    } else if (inst->p1_value != NULL) {
        // Only p1 value is present, MUST execute p2 with it
        // Save it as p2 value and execute
        inst->p2_value = inst->p1_value;
        inst->p1_value = NULL;
        inst->p1_value_ballot = 0;
    } else if (inst->p2_value != NULL) {
        // Only p2 value is present
        // Do phase 2 with it
    } else {
        // There are both p1 and p2 value
		// Compare them
        if (value_cmp(inst->p1_value, inst->p2_value) == 0) {
            // Same value, just delete p1_value
            free(inst->p1_value);
            inst->p1_value = NULL;
            inst->p1_value_ballot = 0;
        } else {
            // Different values
            // p2_value is pushed back to pending list
			carray_push_back(s->values, inst->p2_value);
			// Must execute p2 with p1 value
            inst->p2_value = inst->p1_value;
            inst->p1_value = NULL;
            inst->p1_value_ballot = 0;            
        }
    }
    // Change instance status
    inst->status = p2_pending;
	s->next_accept_iid += 1;
	
	// return values
	*iout = inst->iid;
	*bout = inst->my_ballot;
	*vout = inst->p2_value;
	
	return 1;
}

static void 
do_learn(struct proposer_state* s, accept_ack* ack)
{	
    LOG(DBG, ("Learning outcome of instance %u \n", ack->iid));
	
	struct instance* inst;
	
	// this is quite wheird... we ask for instance iid,
	// but we have to check if the call actually returns
	// the right iid
	inst = proposer_state_get_instance(s, ack->iid);
    if (inst->iid != ack->iid) { 
	    // Instance not even initialized, skip
        return;
    }
    
	int my_val = (inst->p2_value != NULL) &&
        (inst->p2_value->data_size == ack->value_size) &&
        (memcmp(ack->value, inst->p2_value->data, ack->value_size) == 0);

    if (my_val) {
		// Our value accepted, notify client that submitted it
        // vh_notify_client(0, ii->p2_value); //TODO what the hell is that??
    } else if (inst->p2_value != NULL) {
		// Different value accepted, push back our value
		carray_push_back(s->values, inst->p2_value);
        inst->p2_value = NULL;
    } else {
        // We assigned no value to this instance,
        // it comes from somebody else??
    }

	instance_clear(inst);
}

static void 
try_learn(struct proposer_state* s)
{
	accept_ack* ack;
	while ((ack = learner_state_deliver_next(s->learner)) != NULL) {
		do_learn(s, ack);
		free(ack);
	}
}

void
proposer_state_receive_accept(struct proposer_state* s, accept_ack* ack) 
{
	learner_state_receive_accept(s->learner, ack);
	try_learn(s);
}

static void
initialize_instances(struct proposer_state* s, int count)
{
	int i;
	s->instances = carray_new(count);
	assert(s->instances != NULL);	
	for (i = 0; i < carray_size(s->instances); i++)
		carray_push_back(s->instances, instance_new());
}

struct proposer_state*
proposer_state_new(int id, int instances)
{
	struct proposer_state *s;
	s = malloc(sizeof(struct proposer_state));
	s->id = id;	
	s->next_prepare_iid = 0;
    s->next_accept_iid  = 0;
	s->learner = learner_state_new(LEARNER_ARRAY_SIZE);
	if (s->learner == NULL) {
		printf("learner_state_new failed\n");
		return NULL;
	}
	initialize_instances(s, instances);
	s->values = carray_new(1024);
	
	return s;
}

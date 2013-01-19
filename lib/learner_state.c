#include "learner_state.h"
#include "carray.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>


struct instance
{
    iid_t           iid;
    ballot_t        last_update_ballot;
    accept_ack*     acks[N_OF_ACCEPTORS];
    accept_ack*     final_value;
};


struct learner_state
{
	// Highest instance closed (can be higher than current!)
	// TODO: not used
	iid_t highest_iid_closed;
	//Highest instance for which a message was seen
	iid_t highest_iid_seen;
	// Current instance, incremented when current is closed and 
	// the corresponding value is delivered
	iid_t current_iid;
	// the instances we store
	struct carray* instances;
};


static void
instance_clear(struct instance* inst)
{
	assert(inst != NULL);
	memset(inst, 0, sizeof(struct instance));
}

static void
instance_deep_clear(struct instance* inst)
{	
	int i;
    for (i = 0; i < N_OF_ACCEPTORS; i++)
        if (inst->acks[i] != NULL)
            free(inst->acks[i]);
	instance_clear(inst);
}

static struct instance*
instance_new()
{
	struct instance* inst;
	inst = malloc(sizeof(struct instance));
	instance_clear(inst);
	return inst;
}

/*
	Checks if a given instance is closed, that is if a quorum of acceptor
	accepted the same value ballot pair.
	Returns 1 if the instance is closed, 0 otherwise
*/
static int 
instance_has_quorum(struct instance* inst)
{
    accept_ack * curr_ack;
    int i, a_valid_index = -1, count = 0;

	if (inst->final_value != NULL)
		return 1;
    
    //Iterates over stored acks
    for (i = 0; i < N_OF_ACCEPTORS; i++) {
        curr_ack = inst->acks[i];
        
        // skip over missing acceptor acks
        if (curr_ack == NULL)
            continue;
        
        // Count the ones "agreeing" with the last added
        if (curr_ack->ballot == inst->last_update_ballot) {
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
        LOG(DBG, ("Reached quorum, iid: %u is closed!\n", inst->iid));
        inst->final_value = inst->acks[a_valid_index];
        return 1;
    }
    
    //No quorum yet...
    return 0;
}

/*
	Adds the given accept_ack for the given instance.
	Assumes inst->acks[acceptor_id] was already freed.
*/
static void
instance_add_accept(struct instance* inst, accept_ack* ack)
{
	accept_ack * new_ack;
	new_ack = malloc(ACCEPT_ACK_SIZE(ack));
	memcpy(new_ack, ack, ACCEPT_ACK_SIZE(ack));
    inst->acks[ack->acceptor_id] = new_ack;
    inst->last_update_ballot = ack->ballot;
}

/*
	Tries to update the state based on the accept_ack received.
 	Returns 0 if the message was discarded because not relevant,
	otherwise 1.
*/
static int
instance_update(struct instance* inst, accept_ack* ack)
{
	accept_ack* prev_ack;
	
	// First message for this iid
    if (inst->iid == 0) {
        LOG(DBG, ("Received first message for instance: %u\n", ack->iid));
        inst->iid = ack->iid;
        inst->last_update_ballot = ack->ballot;
    }
    assert(inst->iid == ack->iid);
    
    // Instance closed already, drop
    if (instance_has_quorum(inst)) {
		LOG(DBG, ("Dropping accept_ack for iid: %u", ack->iid));
		LOG(DBG, ("already closed\n"));
        return 0;
    }
    
    // No previous message to overwrite for this acceptor
    if (inst->acks[ack->acceptor_id] == NULL) {
        LOG(DBG, ("Got first ack for: %u, acceptor: %d\n", 
			inst->iid, ack->acceptor_id));
        //Save this accept_ack
        instance_add_accept(inst, ack);
        return 1;
    }
    
    // There is already a message from the same acceptor
	prev_ack = inst->acks[ack->acceptor_id];
    
    // Already more recent info in the record, accept_ack is old
    if (prev_ack->ballot >= ack->ballot) {
		LOG(DBG, ("Dropping accept_ack for iid: %u ", ack->iid));
		LOG(DBG, ("stored ballot is newer or equal\n"));
        return 0;
    }
    
    // Replace the previous ack since the received ballot is newer
    LOG(DBG, ("Overwriting previous accept_ack for iid: %u\n", ack->iid));
    free(prev_ack);
    instance_add_accept(inst, ack);

    return 1;
}

static struct instance*
learner_state_get_instance(struct learner_state* s, iid_t iid)
{
	struct instance* inst;
	inst = carray_at(s->instances, iid);
	assert(inst->iid == iid || inst->iid == 0);
	return inst;
}

static struct instance*
learner_state_get_current_instance(struct learner_state* s)
{
	return learner_state_get_instance(s, s->current_iid);
}

accept_ack*
learner_state_deliver_next(struct learner_state* s)
{
	struct instance* inst;
	accept_ack* ack = NULL;
	inst = learner_state_get_current_instance(s);
 	if (instance_has_quorum(inst)) {
		size_t size = ACCEPT_ACK_SIZE(inst->final_value);
		
		// make a copy of the accept_ack to deliver,
		// before clearing the instance
		ack = malloc(size);
		memcpy(ack, inst->final_value, size);

		instance_deep_clear(inst);
		s->current_iid++;
	}
	return ack;
}

void
learner_state_receive_accept(struct learner_state* s, accept_ack* ack)
{
	int relevant;
	struct instance* inst;
	
	// Keep track of highest seen instance id
	if (ack->iid > s->highest_iid_seen) {
		s->highest_iid_seen = ack->iid;
	}

	// Already closed and delivered, ignore message
	if (ack->iid < s->current_iid) {
		LOG(DBG, ("Dropping accept_ack for already delivered iid: %u\n",
			ack->iid));
		return;
	}

	// We are late w.r.t the current iid, ignore message
	// (The instance received is too ahead and will overwrite something)
	if (ack->iid >= s->current_iid + carray_size(s->instances)) {
		LOG(DBG, ("Dropping accept_ack for iid: %u, too far in future\n",
			ack->iid));
		return;
	}

	// Message is within interesting bounds
	// Update the corresponding record

	inst = learner_state_get_instance(s, ack->iid);
	relevant = instance_update(inst, ack);

	if (!relevant) {
		//Not really interesting (i.e. a duplicate message)
		LOG(DBG, ("Learner discarding learn for iid: %u\n", ack->iid));
		return;
	}

	// Message contained some relevant info, 
	// check if instance can be declared closed
	if (!instance_has_quorum(inst)) {
		LOG(DBG, ("Not yet a quorum for iid: %u\n", ack->iid));
		return;
	}

	// Keep track of highest closed
	if (inst->iid > s->highest_iid_closed)
		s->highest_iid_closed = inst->iid;
}

static void
initialize_instances(struct learner_state* s, int count)
{
	int i;
	s->instances = carray_new(count);
	assert(s->instances != NULL);	
	for (i = 0; i < carray_size(s->instances); i++)
		carray_push_back(s->instances, instance_new());
}

struct learner_state*
learner_state_new(int instances)
{
	struct learner_state* s;
	s = malloc(sizeof(struct learner_state));
	initialize_instances(s, instances);
	s->highest_iid_seen = 1;
	s->highest_iid_closed = 0;
	s->current_iid = 1;
	return s;
}

/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "learner.h"
#include "carray.h"
#include "paxos_config.h"
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


struct learner
{
	int late_start;
	iid_t current_iid;
	iid_t highest_iid_seen;
	iid_t highest_iid_closed;
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
instance_has_quorum(struct learner* l, struct instance* inst)
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

static struct instance*
learner_get_instance(struct learner* s, iid_t iid)
{
	struct instance* inst;
	inst = carray_at(s->instances, iid);
	assert(inst->iid == iid || inst->iid == 0);
	return inst;
}

static struct instance*
learner_get_current_instance(struct learner* s)
{
	return learner_get_instance(s, s->current_iid);
}

/*
	Tries to update the state based on the accept_ack received.
*/
static void
learner_update_instance(struct learner* l, accept_ack* ack)
{
	accept_ack* prev_ack;
	struct instance* inst = learner_get_instance(l, ack->iid);
	
	// First message for this iid
	if (inst->iid == 0) {
		LOG(DBG, ("Received first message for instance: %u\n", ack->iid));
		inst->iid = ack->iid;
		inst->last_update_ballot = ack->ballot;
	}
	assert(inst->iid == ack->iid);
    
	// Instance closed already, drop
	if (instance_has_quorum(l, inst)) {
		LOG(DBG, ("Dropping accept_ack for iid %u, already closed\n",
			 ack->iid));
		return;
	}
    
	// No previous message to overwrite for this acceptor
	if (inst->acks[ack->acceptor_id] == NULL) {
		LOG(DBG, ("Got first ack for: %u, acceptor: %d\n", 
		inst->iid, ack->acceptor_id));
		//Save this accept_ack
		instance_add_accept(inst, ack);
		return;
	}
    
	// There is already a message from the same acceptor
	prev_ack = inst->acks[ack->acceptor_id];
    
	// Already more recent info in the record, accept_ack is old
	if (prev_ack->ballot >= ack->ballot) {
		LOG(DBG, ("Dropping accept_ack for iid: %u\n", ack->iid));
		LOG(DBG, ("stored ballot is newer or equal\n"));
		return;
	}
    
	// Replace the previous ack since the received ballot is newer
	LOG(DBG, ("Overwriting previous accept_ack for iid: %u\n", ack->iid));
	free(prev_ack);
	instance_add_accept(inst, ack);
}

accept_ack*
learner_deliver_next(struct learner* s)
{
	struct instance* inst;
	accept_ack* ack = NULL;
	inst = learner_get_current_instance(s);
	if (instance_has_quorum(s, inst)) {
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
learner_receive_accept(struct learner* s, accept_ack* ack)
{
	if (s->late_start) {
		s->late_start = 0;
		s->current_iid = ack->iid;
	}
		
	if (ack->iid > s->highest_iid_seen)
		s->highest_iid_seen = ack->iid;
	
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
	
	learner_update_instance(s, ack);
	
	struct instance* inst = learner_get_instance(s, ack->iid);
	if (instance_has_quorum(s, inst) && (inst->iid > s->highest_iid_closed))
		s->highest_iid_closed = inst->iid;
}

static void
initialize_instances(struct learner* s, int count)
{
	int i;
	s->instances = carray_new(count);
	assert(s->instances != NULL);	
	for (i = 0; i < carray_size(s->instances); i++)
		carray_push_back(s->instances, instance_new());
}

int
learner_has_holes(struct learner* l, iid_t* from, iid_t* to)
{
	if (l->highest_iid_seen > l->current_iid + carray_count(l->instances)) {
		*from = l->current_iid;
		*to = l->highest_iid_seen;
		return 1;
	}
	if (l->highest_iid_closed > l->current_iid) {
		*from = l->current_iid;
		*to = l->highest_iid_closed;
		return 1;
	}
	return 0;
}

struct learner*
learner_new(int instances, int recover)
{
	struct learner* s;
	s = malloc(sizeof(struct learner));
	initialize_instances(s, instances);
	s->current_iid = 1;
	s->highest_iid_seen = 1;
	s->highest_iid_closed = 1;
	s->late_start = !recover;
	return s;
}

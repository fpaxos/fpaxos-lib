/*
	Copyright (c) 2013, University of Lugano
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
    	* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the copyright holders nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
*/


#include "learner.h"
#include "khash.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct instance
{
	iid_t iid;
	ballot_t last_update_ballot;
	accept_ack** acks;
	accept_ack* final_value;
};
KHASH_MAP_INIT_INT(instance, struct instance*);

struct learner
{
	int acceptors;
	int late_start;
	iid_t current_iid;
	iid_t highest_iid_closed;
	khash_t(instance)* instances;
};

static struct instance* learner_get_instance(struct learner* l, iid_t iid);
static struct instance* learner_get_current_instance(struct learner* l);
static struct instance* learner_get_instance_or_create(struct learner* l, iid_t iid);
static void learner_delete_instance(struct learner* l, struct instance* inst);
static struct instance* instance_new(int acceptors);
static void instance_free(struct instance* i, int acceptors);
static void instance_update(struct instance* i, accept_ack* ack, int acceptors);
static int instance_has_quorum(struct instance* i, int acceptors);
static void instance_add_accept(struct instance* i, accept_ack* ack);
static accept_ack* accept_ack_dup(accept_ack* ack);


struct learner*
learner_new(int acceptors)
{
	struct learner* l;
	l = malloc(sizeof(struct learner));
	l->acceptors = acceptors;
	l->current_iid = 1;
	l->highest_iid_closed = 1;
	l->late_start = !paxos_config.learner_catch_up;
	l->instances = kh_init(instance);
	return l;
}

void
learner_free(struct learner* l)
{
	struct instance* inst;
	kh_foreach_value(l->instances, inst, instance_free(inst, l->acceptors));
	kh_destroy(instance, l->instances);
	free(l);
}

void
learner_receive_accept(struct learner* l, accept_ack* ack)
{	
	if (l->late_start) {
		l->late_start = 0;
		l->current_iid = ack->iid;
	}
	
	if (ack->iid < l->current_iid) {
		paxos_log_debug("Dropped accept_ack for iid %u. Already delivered.",
			ack->iid);
		return;
	}
	
	struct instance* inst;
	inst = learner_get_instance_or_create(l, ack->iid);
	
	instance_update(inst, ack, l->acceptors);
	
	if (instance_has_quorum(inst, l->acceptors)
		&& (inst->iid > l->highest_iid_closed))
		l->highest_iid_closed = inst->iid;
}

accept_ack*
learner_deliver_next(struct learner* l)
{
	struct instance* inst = learner_get_current_instance(l);
	if (inst == NULL)
		return NULL;
	if (instance_has_quorum(inst, l->acceptors)) {
		accept_ack* ack = accept_ack_dup(inst->final_value);
		learner_delete_instance(l, inst);
		l->current_iid++;
		return ack;
	}
	return NULL;
}

int
learner_has_holes(struct learner* l, iid_t* from, iid_t* to)
{
	if (l->highest_iid_closed > l->current_iid) {
		*from = l->current_iid;
		*to = l->highest_iid_closed;
		return 1;
	}
	return 0;
}

static struct instance*
learner_get_instance(struct learner* l, iid_t iid)
{
	khiter_t k;
	k = kh_get_instance(l->instances, iid);
	if (k == kh_end(l->instances))
		return NULL;
	return kh_value(l->instances, k);
}

static struct instance*
learner_get_current_instance(struct learner* l)
{
	return learner_get_instance(l, l->current_iid);
}

static struct instance*
learner_get_instance_or_create(struct learner* l, iid_t iid)
{
	struct instance* inst = learner_get_instance(l, iid);
	if (inst == NULL) {
		int rv;
		khiter_t k = kh_put_instance(l->instances, iid, &rv);
		assert(rv != -1);
		inst = instance_new(l->acceptors);
		kh_value(l->instances, k) = inst;
	}
	return inst;
}

static void
learner_delete_instance(struct learner* l, struct instance* inst)
{
	khiter_t k;
	k = kh_get_instance(l->instances, inst->iid);
	kh_del_instance(l->instances, k);
	instance_free(inst, l->acceptors);
}

static struct instance*
instance_new(int acceptors)
{
	int i;
	struct instance* inst;
	inst = malloc(sizeof(struct instance));
	memset(inst, 0, sizeof(struct instance));
	inst->acks = malloc(sizeof(accept_ack*) * acceptors);
	for (i = 0; i < acceptors; ++i)
		inst->acks[i] = NULL;
	return inst;
}

static void
instance_free(struct instance* inst, int acceptors)
{
	int i;
	for (i = 0; i < acceptors; i++)
		if (inst->acks[i] != NULL)
			free(inst->acks[i]);
	free(inst->acks);
	free(inst);
}

static void
instance_update(struct instance* inst, accept_ack* ack, int acceptors)
{	
	if (inst->iid == 0) {
		paxos_log_debug("Received first message for iid: %u", ack->iid);
		inst->iid = ack->iid;
		inst->last_update_ballot = ack->ballot;
	}
	
	if (instance_has_quorum(inst, acceptors)) {
		paxos_log_debug("Dropped accept_ack iid %u. Already closed.", ack->iid);
		return;
	}
	
	accept_ack* prev_ack = inst->acks[ack->acceptor_id];
	if (prev_ack != NULL && prev_ack->ballot >= ack->ballot) {
		paxos_log_debug("Dropped accept_ack for iid %u."
			"Previous ballot is newer or equal.", ack->iid);
		return;
	}
	
	instance_add_accept(inst, ack);
}

/* 
	Checks if a given instance is closed, that is if a quorum of acceptor 
	accepted the same value ballot pair. 
	Returns 1 if the instance is closed, 0 otherwise.
*/
static int 
instance_has_quorum(struct instance* inst, int acceptors)
{
	accept_ack* curr_ack;
	int i, a_valid_index = -1, count = 0;

	if (inst->final_value != NULL)
		return 1;
	
	for (i = 0; i < acceptors; i++) {
		curr_ack = inst->acks[i];
	
		// Skip over missing acceptor acks
		if (curr_ack == NULL) continue;
		
		// Count the ones "agreeing" with the last added
		if (curr_ack->ballot == inst->last_update_ballot) {
			count++;
			a_valid_index = i;
			
			// Special case: an acceptor is telling that
			// this value is -final-, it can be delivered immediately.
			if (curr_ack->is_final) {
				count += acceptors; // For sure >= quorum...
				break;
			}
		}
	}
    
	if (count >= paxos_quorum(acceptors)) {
		paxos_log_debug("Reached quorum, iid: %u is closed!", inst->iid);
		inst->final_value = inst->acks[a_valid_index];
		return 1;
	}
	return 0;
}

/*
	Adds the given accept_ack to the given instance, 
	replacing the previous accept_ack, if any.
*/
static void
instance_add_accept(struct instance* inst, accept_ack* ack)
{
	if (inst->acks[ack->acceptor_id] != NULL)
		free(inst->acks[ack->acceptor_id]);
	inst->acks[ack->acceptor_id] = accept_ack_dup(ack);
	inst->last_update_ballot = ack->ballot;
}

/*
	Returns a copy of it's argument.
*/
static accept_ack*
accept_ack_dup(accept_ack* ack)
{
	accept_ack* copy;
	copy = malloc(ACCEPT_ACK_SIZE(ack));
	memcpy(copy, ack, ACCEPT_ACK_SIZE(ack));
	return copy;
}

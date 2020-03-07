/*
 * Copyright (c) 2013-2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "proposer.h"
#include <instance.h>
#include "carray.h"
#include "quorum.h"
#include "khash.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <paxos_util.h>
#include <proposer_common.h>
#include <paxos_message_conversion.h>
#include <stdbool.h>


KHASH_MAP_INIT_INT(chosen_instances, bool*)
KHASH_MAP_INIT_INT(instance_info, struct standard_proposer_instance_info*)

struct proposer
{
	int id;
	int acceptors;
	int q1;
	int q2;


	// Stuff to handle client values
	struct carray* to_propose_values;
	struct paxos_value* proposed_values;
	unsigned int number_proposed_values;


	iid_t max_trim_iid;
	iid_t next_prepare_iid;
    khash_t(instance_info)* prepare_instance_infos; /* Waiting for prepare acks */
	khash_t(instance_info)* accept_instance_infos;  /* Waiting for accept acks */
	khash_t(chosen_instances)* chosens;
};

struct timeout_iterator
{
	khiter_t pi, ai;
	struct timeval timeout;
	struct proposer* proposer;
};

static void array_remove_at(struct paxos_value* array, unsigned int* array_length, unsigned int index){
    for(unsigned int i = index; i < *array_length - 1; i++)
    //    array[i] = array[i + 1];
            memmove(&array[i], &array[i+1], index * sizeof(struct paxos_value*));  // Safe move

  //  struct paxos_value * tmp = realloc(array, (*array_length - 1) * sizeof(struct paxos_value*) );

   // if (array == 0) {
      //  array = NULL;
   //     array = calloc(1, sizeof(struct paxos_value));
   //     *array_length = 0;
   // } else {
      //  array = tmp;
        *array_length = *array_length - 1;
  //  }
    //if (tmp == NULL && array_length > 1) {
        /* No memory available */
   //     exit(EXIT_FAILURE);
  //  }
 //   array = tmp;
}

//static struct ballot proposer_next_ballot(struct proposer* p, ballot_t b);
static void proposer_preempt(struct proposer* p, struct standard_proposer_instance_info* inst,
	paxos_prepare* out);
static void proposer_move_proposer_instance_info(khash_t(instance_info)* f, khash_t(instance_info)* t,
                                                 struct standard_proposer_instance_info* inst, int quorum_size);
static void proposer_trim_proposer_instance_infos(struct proposer* p, khash_t(instance_info)* h,
	iid_t iid);


void check_and_handle_promises_value(struct paxos_promise *ack, struct standard_proposer_instance_info *inst);

struct proposer*
proposer_new(int id, int acceptors, int q1, int q2)
{
	struct proposer *p;
	p = calloc(1, sizeof(struct proposer));
	p->id = id;
	p->acceptors = acceptors;
	p->q1 = q1;
	p->q2 = q2;
	p->max_trim_iid = 0;
	p->next_prepare_iid = 1;
	p->to_propose_values = carray_new(5000);
	p->proposed_values = calloc(0, sizeof(struct paxos_value));
	p->number_proposed_values = 0;
	p->prepare_instance_infos = kh_init(instance_info);
	p->accept_instance_infos = kh_init(instance_info);
	p->chosens = kh_init(chosen_instances);
	return p;
}

static void standard_proposer_instance_info_free(struct standard_proposer_instance_info* inst) {
    proposer_common_instance_info_free(&inst->common_info);
    quorum_destroy(&inst->quorum);
}

void
proposer_free(struct proposer* p)
{
	struct standard_proposer_instance_info* inst;
	bool* boolean;
	kh_foreach_value(p->prepare_instance_infos, inst, standard_proposer_instance_info_free(inst));
	kh_foreach_value(p->accept_instance_infos, inst, standard_proposer_instance_info_free(inst));
	kh_foreach_value(p->chosens, boolean, free(boolean));
	kh_destroy(instance_info, p->prepare_instance_infos);
	kh_destroy(instance_info, p->accept_instance_infos);
	carray_foreach(p->to_propose_values, carray_paxos_value_free);
	carray_free(p->to_propose_values);
	free(p);
}



static struct standard_proposer_instance_info*
proposer_instance_info_new(iid_t iid, struct ballot ballot, int acceptors, int q1)
{
    struct standard_proposer_instance_info* inst;
    inst = calloc(1, sizeof(struct standard_proposer_instance_info));
    inst->common_info = proposer_common_info_new(iid, ballot);
    quorum_init(&inst->quorum, acceptors,q1);
    assert(inst->common_info.iid > 0);
    return inst;
}

void
proposer_add_client_value_to_queue(struct proposer* p, const char* value, size_t size)
{
	paxos_value* v;
	v = paxos_value_new(value, size);
	carray_push_back(p->to_propose_values, v);
}

int
proposer_prepared_count(struct proposer* p)
{
	return kh_size(p->prepare_instance_infos);
}

void proposer_next_instance(struct proposer* p) {
    p->next_prepare_iid++;
    paxos_log_debug("Incrementing current Instance. Now at instance %u", p->next_prepare_iid);
}

uint32_t proposer_get_current_instance(struct proposer* p) {
    return p->next_prepare_iid;
}


static bool is_instance_chosen(struct proposer* p, iid_t instance) {
    khiter_t k = kh_get_chosen_instances(p->chosens, instance);

    if (kh_size(p->chosens) == 0) {
        return false;
    } else if (k == kh_end(p->chosens)) {
    //    paxos_log_debug("Instance: %u not chosen", instance);
        return false;
    } else {
      //  paxos_log_debug("Instance %u chosen", instance);
        return true;
    }
}

iid_t proposer_get_next_instance_to_prepare(struct proposer* p) {
    // min instance that is both not chosen and not inited
    iid_t current_min_instance = p->max_trim_iid + 1;

    if (!is_instance_chosen(p, current_min_instance) && kh_size(p->prepare_instance_infos) == 0 && kh_size(p->accept_instance_infos) == 0)
        return 1;
    current_min_instance++;
    khiter_t preprared_key = kh_get_instance_info(p->prepare_instance_infos, current_min_instance);
    khiter_t accept_key = kh_get_instance_info(p->accept_instance_infos, current_min_instance);
    while(is_instance_chosen(p, current_min_instance) || preprared_key != kh_end(p->prepare_instance_infos) || accept_key != kh_end(p->accept_instance_infos)) { //kh_end also is used for not found
        current_min_instance++;
        preprared_key = kh_get_instance_info(p->prepare_instance_infos, current_min_instance);
        accept_key = kh_get_instance_info(p->accept_instance_infos, current_min_instance);
    }
    return current_min_instance;
}

uint32_t proposer_get_min_unchosen_instance(struct proposer* p) {
    iid_t current_min_instance = 1;

    if (kh_size(p->chosens) == 0)
        return current_min_instance;

    khiter_t key = kh_get_chosen_instances(p->chosens, current_min_instance);
    while(key != kh_end(p->chosens)) { //kh_end also is used for not found
        current_min_instance++;
        key = kh_get_chosen_instances(p->chosens, current_min_instance);
    }
    return current_min_instance;
}


static void set_instance_chosen(struct proposer* p, iid_t instance) {
    khiter_t k = kh_get_chosen_instances(p->chosens, instance);

    if (k == kh_end(p->chosens)) {
        bool* chosen = calloc(1, sizeof(bool));
        *chosen = true;
        int rv;
        k = kh_put_chosen_instances(p->chosens, instance, &rv);
        assert(rv > 0);
        kh_value(p->chosens, k) = chosen;
        paxos_log_debug("Instance %u set to chosen", instance);
    }
}

void
proposer_prepare(struct proposer* p, iid_t instance, paxos_prepare* out) {
    if (is_instance_chosen(p, instance)) {
        paxos_log_debug("Instance %u already chosen so skipping", instance);
        proposer_next_instance(p);
    }

    struct standard_proposer_instance_info* inst;
    unsigned int k = kh_get_instance_info(p->prepare_instance_infos, instance);

    if (k == kh_end(p->prepare_instance_infos)) {
        // New instance
        struct ballot ballot = (struct ballot) {.number = rand() % 20, .proposer_id = p->id};

        int rv;
        inst = proposer_instance_info_new(instance, ballot, p->acceptors, p->q1);
        k = kh_put_instance_info(p->prepare_instance_infos, instance, &rv);
        assert(rv > 0);
        kh_value(p->prepare_instance_infos, k) = inst;
        *out = (struct paxos_prepare) {.iid = inst->common_info.iid, .ballot = inst->common_info.ballot};
    }
}



int
proposer_receive_promise(struct proposer* p, paxos_promise* ack,
	paxos_prepare* out)
{
    assert(ack->ballot.proposer_id == p->id);
    if (is_instance_chosen(p, ack->iid)) {
        paxos_log_debug("Promise dropped, Instance %u chosen", ack->iid);
        return 0;

    }
	khiter_t k = kh_get_instance_info(p->prepare_instance_infos, ack->iid);

	if (k == kh_end(p->prepare_instance_infos)) {
		paxos_log_debug("Promise dropped, Instance %u not pending", ack->iid);
		return 0;
	}
	struct standard_proposer_instance_info* inst = kh_value(p->prepare_instance_infos, k);


	if (ballot_greater_than(inst->common_info.ballot, ack->ballot)) {
        paxos_log_debug("Promise dropped, too old");
        return 0;
    }

	assert(ballot_greater_than_or_equal(inst->common_info.ballot, ack->ballot));

	if (quorum_add(&inst->quorum, ack->aid) == 0) {
		paxos_log_debug("Duplicate promise dropped from: %d, iid: %u",
			ack->aid, inst->common_info.iid);
		return 0;
	}

	paxos_log_debug("Received valid promise from: %d, iid: %u",
		ack->aid, inst->common_info.iid);


    check_and_handle_promises_value(ack, inst);

    if (quorum_reached(&inst->quorum))
        return 1;
	return 0;
}

void check_and_handle_promises_value(paxos_promise *ack, struct standard_proposer_instance_info *inst) {
    if (ack->value.paxos_value_len > 0) {
        paxos_log_debug("Promise has value");
        if (ballot_greater_than_or_equal(ack->value_ballot, inst->common_info.value_ballot)) {
            if (proposer_instance_info_has_promised_value(&inst->common_info))
                paxos_value_free(inst->common_info.promised_value);
            copy_ballot(&inst->common_info.value_ballot, &ack->value_ballot);
            inst->common_info.promised_value = calloc(1, sizeof(inst->common_info.promised_value));

            paxos_value_copy(inst->common_info.promised_value, &ack->value);
            //inst->common_info.promised_value = paxos_value_new(ack->value.paxos_value_val,
            //	ack->value.paxos_value_len);
            paxos_log_debug("Value in promise saved, removed older value");
        } else
            paxos_log_debug("Value in promise ignored");
    }
}

/*
 *
 * TODO Cant do this till promise and acceptances quorums are added
 * There is no storage for each promise
bool is_instance_chosen_in_promise_phase(struct proposer* p, iid_t instance){
    khiter_t k = kh_get_instance_info(p->prepare_instance_infos, instance);

    struct standard_proposer_instance_info* instance_info = kh_value(p->prepare_instance_infos, k);
    if (k != kh_end(p->prepare_instance_infos)) {
        if (quorum_reached(&instance_info->quorum)) {

            for (int i = 0; i < p->q1; i++) {
                if (instance_info->quorum)

            }
        }
    }
}
*/


int
proposer_accept(struct proposer* p, paxos_accept* out)
{
	khiter_t k;
	struct standard_proposer_instance_info* inst = NULL;
	khash_t(instance_info)* h = p->prepare_instance_infos;

	// THis means to find the current smallest instance to send acceptances for (that hasn't already been chosen)
	// Find smallest inst->common_info.iid
	for (k = kh_begin(h); k != kh_end(h); ++k) {
		if (!kh_exist(h, k))
			continue;
		else if (inst == NULL || inst->common_info.iid > kh_value(h, k)->common_info.iid)
			inst = kh_value(h, k);

	}



    if (inst == NULL || !quorum_reached(&inst->quorum))
        return 0;

    if (is_instance_chosen(p, inst->common_info.iid)) {
        paxos_log_debug("Instance %u already chosen so skipping", inst->common_info.iid);
        standard_proposer_instance_info_free(inst);
        return 0;
    }

	paxos_log_debug("Trying to accept iid %u", inst->common_info.iid);

	// Is there a value to accept?

	// Change to only add the client value if the promised value is not there
	//if (!proposer_instance_info_has_value(&inst->common_info))
//		inst->common_info.value = carray_pop_front(p->values);
  //  if (!proposer_instance_info_has_value(&inst->common_info) && !proposer_instance_info_has_promised_value(&inst->common_info)) {
	//	paxos_log_debug("Proposer: No value to accept");
//		return 0;
//	}
    if (!proposer_instance_info_has_promised_value(&inst->common_info)){
        if (!carray_empty(p->to_propose_values)) {
            struct paxos_value* value_to_propose =  carray_pop_front(p->to_propose_values);
            inst->common_info.value = calloc(1, sizeof(struct paxos_value));
            inst->common_info.value = value_to_propose;
            p->number_proposed_values++;

            p->proposed_values = realloc(p->proposed_values, sizeof(struct paxos_value) * p->number_proposed_values);
            p->proposed_values[p->number_proposed_values - 1] = *value_to_propose;
       //     paxos_value_free(value_to_propose);
        } else {
            paxos_log_debug("Proposer: No value to accept");

            return 0;
        }
    }
//    inst->common_info.value = calloc(1, sizeof(struct paxos_value*));
//    struct paxos_value* asdf = calloc(1, sizeof(struct paxos_value*));
//    *asdf = (struct paxos_value){2, "OK"};
//    inst->common_info.value = asdf;


	// We have both a prepared standard_proposer_instance_info and a value
	unsigned int size_prepares = kh_size(p->prepare_instance_infos);
    unsigned int size_accepts = kh_size(p->accept_instance_infos);
	proposer_move_proposer_instance_info(p->prepare_instance_infos, p->accept_instance_infos, inst, p->q2);
	assert(size_accepts == (kh_size(p->accept_instance_infos)  - 1));
	assert(size_prepares == (kh_size(p->prepare_instance_infos) + 1));
	proposer_instance_info_to_accept(&inst->common_info, out);
	return 1;
}

int
proposer_receive_accepted(struct proposer* p, paxos_accepted* ack, struct paxos_chosen* chosen)
{
    assert(ack->promise_ballot.proposer_id == p->id);


    if (is_instance_chosen(p, ack->iid)) {
        paxos_log_debug("Acceptance dropped, Instance %u chosen", ack->iid);
        return 0;
    }

	khiter_t k = kh_get_instance_info(p->accept_instance_infos, ack->iid);

    if (k == kh_end(p->accept_instance_infos)) {
		paxos_log_debug("Accept ack dropped, iid: %u not pending", ack->iid);
		return 0;
	}

	struct standard_proposer_instance_info* inst = kh_value(p->accept_instance_infos, k);
	if (ballot_equal(&ack->promise_ballot, inst->common_info.ballot)) {
		if (!quorum_add(&inst->quorum, ack->aid)) {
			paxos_log_debug("Duplicate accept dropped from: %d, iid: %u",
				ack->aid, inst->common_info.iid);
			return 0;
		}

		if (quorum_reached(&inst->quorum)) {
		    assert(ballot_equal(&ack->promise_ballot, ack->value_ballot));
            paxos_chosen_from_paxos_accepted(chosen, ack);
            assert(ballot_equal(&ack->promise_ballot, chosen->ballot));
            assert(ballot_equal(&ack->value_ballot, chosen->ballot));
            assert(chosen->iid == ack->iid);
            proposer_receive_chosen(p, chosen);
            return 1;
        }
	}
    return 0;
}

int proposer_receive_chosen(struct proposer* p, struct paxos_chosen* ack) {
    if (is_instance_chosen(p, ack->iid)) {
        paxos_log_debug("Chosen message dropped, Instance %u already known to be chosen", ack->iid);
        return 0;
    }

    paxos_log_debug("Received chosen message for Instance %u", ack->iid);
    set_instance_chosen(p, ack->iid);
    assert(is_instance_chosen(p, ack->iid));

    khiter_t k = kh_get_instance_info(p->accept_instance_infos, ack->iid);

    if (k != kh_end(p->accept_instance_infos)) {

        struct standard_proposer_instance_info *inst = kh_value(p->accept_instance_infos, k);
      //  if (quorum_reached(&inst->quorum)) {
            if (proposer_instance_info_has_value(&inst->common_info)) {
//                    paxos_value_cmp(inst->common_info.value, inst->common_info.promised_value) != 0) {
//                    carray_push_back(p->values, inst->common_info.value);
                //        inst->common_info.value = NULL;

                // Remove from proposed values
                for (unsigned int i = 0; i < p->number_proposed_values; i ++) {
                    if (is_values_equal(*inst->common_info.value, p->proposed_values[i])) {
                        array_remove_at(p->proposed_values, &p->number_proposed_values, i);
                    }
                }

            //    paxos_value_free(inst->common_info.value);
//            }

                kh_del_instance_info(p->accept_instance_infos, k);
                standard_proposer_instance_info_free(inst);
            }
    //    }

        k = kh_get_instance_info(p->prepare_instance_infos, ack->iid);
        if (k != kh_end(p->prepare_instance_infos)) {
            inst = kh_value(p->accept_instance_infos, k);
            kh_del_instance_info(p->prepare_instance_infos, k);
            standard_proposer_instance_info_free(inst);
        }

    }

  //  if (proposer_get_current_instance(p) == ack->iid)
    //    proposer_next_instance(p);
    return 1;
}

/*
int
is_proposer_instance_pending_and_message_return(struct proposer* p, paxos_preempted* ack,
                                                paxos_prepare* out)
{
	khiter_t k = kh_get_instance_info(p->accept_instance_infos, ack->iid);

	// check if being accepted
	if (k == kh_end(p->accept_instance_infos)) {

	    // not then check if trying to get promise
        k = kh_get_instance_info(p->prepare_instance_infos, ack->iid);

        if (k == kh_end(p->prepare_instance_infos)) {
            paxos_log_debug("Promise dropped, standard_proposer_instance_info %u not pending", ack->iid);
            return 0;
        }

        struct standard_proposer_instance_info* inst = kh_value(p->prepare_instance_infos, k);

        if (ballot_greater_than_or_equal(ack->ballot, inst->common_info.ballot)) {
            paxos_log_debug("Instance %u preempted in Promise Phase: ballot %d ack ballot %d",
                            inst->common_info.iid, inst->common_info.ballot, ack->ballot);
            // todo current ballot from ack then adjust
           // proposer_preempt(p, inst, out);
            return 1;
        }

	} else {

        struct standard_proposer_instance_info *inst = kh_value(p->accept_instance_infos, k);

        if (ballot_greater_than(ack->ballot, inst->common_info.ballot)) {
            paxos_log_debug("nstance %u preempted in Acceptance Phase: ballot %d ack ballot %d",
                            inst->common_info.iid, inst->common_info.ballot, ack->ballot);
            if (proposer_instance_info_has_promised_value(&inst->common_info))
                paxos_value_free(inst->common_info.promised_value);
            proposer_move_proposer_instance_info(p->accept_instance_infos, p->prepare_instance_infos, inst, p->q1);
         //   proposer_preempt(p, inst, out); // todo make ballot greater than one receieved
            standard_proposer_instance_info_free(inst);
            return 1;
        } else {
            return 0;
        }
    }
	return 0;
}*/

struct timeout_iterator*
proposer_timeout_iterator(struct proposer* p)
{
	struct timeout_iterator* iter;
	iter = malloc(sizeof(struct timeout_iterator));
	iter->pi = kh_begin(p->prepare_instance_infos);
	iter->ai = kh_begin(p->accept_instance_infos);
	iter->proposer = p;
	gettimeofday(&iter->timeout, NULL);
	return iter;
}

static struct standard_proposer_instance_info*
next_timedout(khash_t(instance_info)* h, khiter_t* k, struct timeval* t)
{
	for (; *k != kh_end(h); ++(*k)) {
		if (!kh_exist(h, *k))
			continue;
		struct standard_proposer_instance_info* inst = kh_value(h, *k);
		if (quorum_reached(&inst->quorum))
			continue;
		if (proposer_instance_info_has_timedout(&inst->common_info, t))
			return inst;
	}
	return NULL;
}

int
timeout_iterator_prepare(struct timeout_iterator* iter, paxos_prepare* out)
{
	struct standard_proposer_instance_info* inst;
	struct proposer* p = iter->proposer;
	inst = next_timedout(p->prepare_instance_infos, &iter->pi, &iter->timeout);
	if (inst == NULL)
		return 0;
	*out = (struct paxos_prepare){inst->common_info.iid, {inst->common_info.ballot.number, inst->common_info.ballot.proposer_id}};
	inst->common_info.created_at = iter->timeout;
	return 1;
}

int
timeout_iterator_accept(struct timeout_iterator* iter, paxos_accept* out)
{
	struct standard_proposer_instance_info* inst;
	struct proposer* p = iter->proposer;
	inst = next_timedout(p->accept_instance_infos, &iter->ai, &iter->timeout);
	if (inst == NULL)
		return 0;

   // inst->common_info.ballot = (struct ballot) {.number = inst->common_info.ballot.number, .proposer_id = p->id};
    //inst->common_info.value_ballot = (struct ballot) {.number = 0, .proposer_id = p->id};
    //inst->common_info.promised_value = NULL;
   // quorum_clear(&inst->quorum);

    gettimeofday(&inst->common_info.created_at, NULL);

	proposer_instance_info_to_accept(&inst->common_info, out);
	inst->common_info.created_at = iter->timeout;
	return 1;

//    struct p* inst
//    struct proposer* p = iter->proposer;
//    inst = next_timedout(p->accept_instance_infos, &iter->ai, &iter->timeout);
//    if (inst == NULL)
//        return 0;
//    proposer_instance_info_to_accept(inst, out);
//    inst->created_at = iter->timeout;
//    return 1;
}

void
timeout_iterator_free(struct timeout_iterator* iter)
{
	free(iter);
}

int proposer_receive_preempted(struct proposer* p, struct paxos_preempted* preempted, struct paxos_prepare* out) {
    if (is_instance_chosen(p, preempted->iid)) {
        paxos_log_debug("Ignoring preempted, instance %u already known to be chosen", preempted->iid);
          return 0;
    }

    // check if trying to get promises
    khiter_t  k = kh_get_instance_info(p->prepare_instance_infos,preempted->iid);

    if (k != kh_end(p->prepare_instance_infos)) {
        struct standard_proposer_instance_info* inst = kh_value(p->prepare_instance_infos, k);
        if (ballot_equal(&preempted->ballot, inst->common_info.ballot)) {
            // current ballot was preempted
            paxos_log_debug("Instance %u preempted at ballot %u.%u in Promise Phase", inst->common_info.iid,
                            inst->common_info.ballot.number, inst->common_info.ballot.proposer_id);
            proposer_preempt(p, inst, out);
            return 1;
        }
    }


    k = kh_get_instance_info(p->accept_instance_infos, preempted->iid);
    // check if being accepted
    if (k != kh_end(p->accept_instance_infos)) {
        struct standard_proposer_instance_info *inst = kh_value(p->accept_instance_infos, k);

        if (ballot_equal(&preempted->ballot, inst->common_info.ballot)) {
            paxos_log_debug("Instance %u preempted in Acceptance Phase: ballot %d ack ballot %d",
                            inst->common_info.iid, inst->common_info.ballot, preempted->ballot);
                proposer_preempt(p, inst, out);
                // todo check if it was in the list of proposed values from clients
                // if it was return it to the queues front.
                // else drop it
                for (unsigned int i = 0; i < p->number_proposed_values; i ++) {
                    if (is_values_equal(*inst->common_info.value, p->proposed_values[i])) {
                        struct paxos_value proposed_value = p->proposed_values[i];
                        struct paxos_value* to_add_back = calloc(1, sizeof(struct paxos_value));
                        copy_value(&proposed_value, to_add_back);
                        carray_push_front(p->to_propose_values, &proposed_value);
                        array_remove_at(p->proposed_values, &p->number_proposed_values, i);

                    }
                }
                inst->common_info.value = NULL;
                proposer_move_proposer_instance_info(p->accept_instance_infos, p->prepare_instance_infos, inst, p->q1);

            return 1;
        }
    }

    return 0;
}

void
proposer_preempt(struct proposer* p, struct standard_proposer_instance_info* inst, paxos_prepare* out)
{

	inst->common_info.ballot = (struct ballot) {.number = inst->common_info.ballot.number + (rand() % 20), .proposer_id = p->id};
//	inst->common_info.value_ballot = (struct ballot) {.number = 0, .proposer_id = p->id};
//	inst->common_info.promised_value = NULL;
	quorum_clear(&inst->quorum);
	*out = (paxos_prepare) {inst->common_info.iid, (struct ballot) {.number = inst->common_info.ballot.number, .proposer_id = inst->common_info.ballot.proposer_id}};
	gettimeofday(&inst->common_info.created_at, NULL);
}

static void
proposer_move_proposer_instance_info(khash_t(instance_info)* f, khash_t(instance_info)* t,
                                     struct standard_proposer_instance_info* inst, int quorum_size)
{
	int rv;
	khiter_t k;
	k = kh_get_instance_info(f, inst->common_info.iid);
	assert(k != kh_end(f));
	kh_del_instance_info(f, k);
	k = kh_put_instance_info(t, inst->common_info.iid, &rv);
	assert(rv > 0);
	kh_value(t, k) = inst;
	quorum_resize(&inst->quorum, quorum_size);
}

static void
proposer_trim_proposer_instance_infos(struct proposer* p, khash_t(instance_info)* h, iid_t iid)
{
	khiter_t k;
	for (k = kh_begin(h); k != kh_end(h); ++k) {
		if (!kh_exist(h, k))
			continue;
		struct standard_proposer_instance_info* inst = kh_value(h, k);
		if (inst->common_info.iid <= iid) {
			if (proposer_instance_info_has_value(&inst->common_info)) {
				carray_push_back(p->to_propose_values, inst->common_info.value);
				inst->common_info.value = NULL;
			}
			kh_del_instance_info(h, k);
            standard_proposer_instance_info_free(inst);
		}
	}
}



void
proposer_set_current_instance(struct proposer* p, iid_t iid)
{
    if (iid >= p->next_prepare_iid) {
        p->next_prepare_iid = iid;
        // remove proposer_instance_infos older than iid
        if (iid <= proposer_get_min_unchosen_instance(p)) {
            proposer_trim_proposer_instance_infos(p, p->prepare_instance_infos, iid);
            proposer_trim_proposer_instance_infos(p, p->accept_instance_infos, iid);
        }
    }
}



void
proposer_receive_acceptor_state(struct proposer* p, paxos_standard_acceptor_state* state)
{
    if (p->max_trim_iid < state->trim_iid) {
        paxos_log_debug("Received new acceptor state, %u trim_iid from %u", state->trim_iid, state->aid);
        p->max_trim_iid = state->trim_iid;
        proposer_set_current_instance(p, state->trim_iid);
    }
}

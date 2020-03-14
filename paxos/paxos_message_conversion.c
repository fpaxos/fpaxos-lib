//
// Created by Michael Davis on 11/12/2019.
//

//
// Created by Michael Davis on 11/12/2019.
//


#include "standard_acceptor.h"
#include "standard_stable_storage.h"
#include "ballot.h"
#include "paxos_value.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <paxos_storage.h>
#include <paxos_types.h>
#include <assert.h>


void copy_epoch_ballot(const struct epoch_ballot *src, struct epoch_ballot *dst) {
    *dst = (struct epoch_ballot) {.epoch = src->epoch, .ballot = src->ballot};
}


void
paxos_accepted_to_promise(const struct paxos_accepted *acc, standard_paxos_message *out) {
    out->type = PAXOS_PROMISE;
    out->u.promise.aid = acc->aid;
    out->u.promise.iid = acc->iid;
    copy_ballot(&acc->promise_ballot, &out->u.promise.ballot);//out->u.promise.ballot = acc->ballot;
    copy_ballot(&acc->value_ballot, &out->u.promise.value_ballot);
    copy_value(&acc->value, &out->u.promise.value);
   /* out->u.promise = (paxos_promise) {
            acc->aid,
            acc->iid,
            acc->ballot,
            acc->value_ballot,
            {acc->value.paxos_value_len, acc->value.paxos_value_val}
    };*/
}

void
paxos_accept_to_accepted(int id, const struct paxos_accept *acc, standard_paxos_message *out) {
    out->type = PAXOS_ACCEPTED;
    out->u.accepted.iid = acc->iid;
    out->u.accepted.aid = id;
    copy_ballot(&acc->ballot, &out->u.accepted.promise_ballot);
    copy_ballot(&acc->ballot, &out->u.accepted.value_ballot);
    copy_value(&acc->value, &out->u.accepted.value);
    /*char *value = NULL;
    int value_size = acc->value.paxos_value_len;
    if (value_size > 0) {
        value = malloc(value_size);
        memcpy(value, acc->value.paxos_value_val, value_size);
    }
    out->type = PAXOS_ACCEPTED;
    out->u.accepted = (paxos_accepted) {
            id,
            acc->iid,
            acc->ballot,
            acc->ballot,
            {value_size, value}
    };
     */
}

void // should this return the same ballot or a higher one?
union_paxos_prepare_to_preempted(int id, const struct paxos_prepare* prepare, struct standard_paxos_message *out) {
    out->type = PAXOS_PREEMPTED;
    out->u.preempted = (struct paxos_preempted) {
        .aid = id,
        .iid = prepare->iid,
        .ballot = prepare->ballot
    };
}

void
paxos_accept_to_preempted(int id, const struct paxos_accept* accept, standard_paxos_message *out){
    out->type = PAXOS_PREEMPTED;
    out->u.preempted = (paxos_preempted) {id, accept->iid, accept->ballot};
}

void
paxos_accepted_to_preempted(int id, const struct paxos_accepted *acc, standard_paxos_message *out) {
    out->type = PAXOS_PREEMPTED;
    out->u.preempted = (paxos_preempted) {id, acc->iid, acc->promise_ballot};
}

void
paxos_accepted_to_accept(const struct paxos_accepted *accepted, paxos_accept *out) {
 //   out->ballot = accepted->value_ballot;
    copy_ballot(&accepted->value_ballot, &out->ballot);
    out->iid = accepted->iid;

    copy_value(&accepted->value, &out->value);

}
void
paxos_accepted_to_prepare(const struct paxos_accepted *accepted, paxos_prepare *out) {
    //out->ballot = accepted->ballot;
    copy_ballot(&accepted->promise_ballot, &out->ballot);
    out->iid = accepted->iid;
}

void
union_paxos_promise_from_accept_and_prepare(const struct paxos_prepare* prepare, const struct paxos_accept* accept, const int aid, struct standard_paxos_message* out){
    out->type = PAXOS_PROMISE;
    struct paxos_value copied_value;
    copy_value(&accept->value, &copied_value);
    out->u.promise = (struct paxos_promise) {
            .aid = aid,
            .iid = prepare->iid,
            .ballot = prepare->ballot,
            .value_ballot = accept->ballot,
            .value = {copied_value.paxos_value_len, copied_value.paxos_value_val}
    };
}

void epoch_ballot_accept_from_paxos_accepted(const struct paxos_accepted* accepted, uint32_t accept_epoch, struct epoch_ballot_accept* epoch_ballot_accept){
    epoch_ballot_accept->instance = accepted->iid;
    epoch_ballot_accept->epoch_ballot_requested.epoch = accept_epoch;
    epoch_ballot_accept->epoch_ballot_requested.ballot = accepted->value_ballot;
    copy_value(&accepted->value, &epoch_ballot_accept->value_to_accept);
}

void paxos_accepted_from_epoch_ballot_accept(const struct epoch_ballot_accept* epoch_ballot_accept, int aid, struct paxos_accepted* accepted){
    accepted->aid = aid;
    accepted->iid = epoch_ballot_accept->instance;
    accepted->promise_ballot = epoch_ballot_accept->epoch_ballot_requested.ballot;
    accepted->value_ballot = epoch_ballot_accept->epoch_ballot_requested.ballot;
    copy_value(&epoch_ballot_accept->value_to_accept, &accepted->value);
}

void paxos_prepare_from_epoch_ballot_prepare(const struct epoch_ballot_prepare* epoch_ballot_prepare, struct paxos_prepare* out) {
    out->ballot = epoch_ballot_prepare->epoch_ballot_requested.ballot;
    out->iid = epoch_ballot_prepare->instance;
}


void union_epoch_ballot_promise_from_epoch_ballot_accept_and_paxos_prepare(struct writeahead_epoch_paxos_message* message_to_be_promise, const struct paxos_prepare* prepare, const struct epoch_ballot_accept* accept, int aid, uint32_t promised_epoch){
    assert(prepare->iid == accept->instance);
    message_to_be_promise->type = WRITEAHED_EPOCH_BALLOT_PROMISE;
    message_to_be_promise->message_contents.epoch_ballot_promise.acceptor_id = aid;
    message_to_be_promise->message_contents.epoch_ballot_promise.instance = prepare->iid;
    message_to_be_promise->message_contents.epoch_ballot_promise.promised_epoch_ballot = (struct epoch_ballot){.epoch = promised_epoch, .ballot = prepare->ballot};
    message_to_be_promise->message_contents.epoch_ballot_promise.last_accepted_ballot = (struct epoch_ballot) {.epoch = accept->epoch_ballot_requested.epoch, .ballot = accept->epoch_ballot_requested.ballot};
    copy_value(&accept->value_to_accept, &message_to_be_promise->message_contents.epoch_ballot_promise.last_accepted_value);
}



void union_epoch_ballot_promise_from_epoch_ballot_accept_and_epoch_ballot_prepare(struct writeahead_epoch_paxos_message* message_to_be_promise, const struct epoch_ballot_prepare* prepare, const struct epoch_ballot_accept* accept, int aid){
    assert(prepare->instance == accept->instance);
    message_to_be_promise->type = WRITEAHED_EPOCH_BALLOT_PROMISE;
    message_to_be_promise->message_contents.epoch_ballot_promise.acceptor_id = aid;
    message_to_be_promise->message_contents.epoch_ballot_promise.instance = prepare->instance;
    message_to_be_promise->message_contents.epoch_ballot_promise.promised_epoch_ballot = (struct epoch_ballot){.epoch = prepare->epoch_ballot_requested.epoch, .ballot = prepare->epoch_ballot_requested.ballot};
    message_to_be_promise->message_contents.epoch_ballot_promise.last_accepted_ballot = (struct epoch_ballot) {.epoch = accept->epoch_ballot_requested.epoch, .ballot = accept->epoch_ballot_requested.ballot};
    copy_value(&accept->value_to_accept, &message_to_be_promise->message_contents.epoch_ballot_promise.last_accepted_value);
}

void union_epoch_ballot_accepted_from_epoch_ballot_accept(struct writeahead_epoch_paxos_message* message_to_be_accepted, const struct epoch_ballot_accept *accept, int acceptor_id) {
    message_to_be_accepted->type = WRITEAHEAD_EPOCH_BALLOT_ACCEPTED;
    message_to_be_accepted->message_contents.epoch_ballot_accepted.acceptor_id = acceptor_id;
    message_to_be_accepted->message_contents.epoch_ballot_accepted.instance =  accept->instance;
    message_to_be_accepted->message_contents.epoch_ballot_accepted.accepted_epoch_ballot = (struct epoch_ballot) {.epoch = accept->epoch_ballot_requested.epoch, .ballot = accept->epoch_ballot_requested.ballot};
    copy_value(&accept->value_to_accept, &message_to_be_accepted->message_contents.epoch_ballot_accepted.accepted_value);
}

void paxos_accept_from_epoch_ballot_accept(const struct epoch_ballot_accept* eb_accept, struct paxos_accept* converted_accept){
    // NOTE: THIS DOES NOT COPY OVER THE EPOCH_ --- PLEASE ENSURE IT IS PRESERVED IF THE EB_ACCEPT IS GOING TO BE STORED!!!!
    converted_accept->iid = eb_accept->instance;
    converted_accept->ballot = eb_accept->epoch_ballot_requested.ballot;
    copy_value(&eb_accept->value_to_accept, &converted_accept->value);
}

void epoch_ballot_preempted_from_epoch_ballot_requested_and_epoch_ballot_last_responded(int aid, const iid_t instance, const struct epoch_ballot* requested_epoch_ballot, const struct epoch_ballot* last_responded_epoch_ballot, struct epoch_ballot_preempted* preempted) {
    // here is where we could fingure out the type of premption for a flag
    preempted->acceptor_id = aid;
    preempted->instance = instance;
    copy_epoch_ballot(requested_epoch_ballot, &preempted->requested_epoch_ballot);
    preempted->acceptors_current_epoch_ballot = (struct epoch_ballot) {.epoch = last_responded_epoch_ballot->epoch, .ballot = last_responded_epoch_ballot->ballot};

}

void union_epoch_ballot_preempted_from_epoch_ballot_preempted(const struct epoch_ballot_preempted* eb_preempte, struct writeahead_epoch_paxos_message* message_to_be_preempt) {
    message_to_be_preempt->type = WRITEAHEAD_EPOCH_BALLOT_PREEMPTED;
    message_to_be_preempt->message_contents.epoch_ballot_preempted.acceptor_id = eb_preempte->acceptor_id;
    message_to_be_preempt->message_contents.epoch_ballot_preempted.instance = eb_preempte->instance;
    message_to_be_preempt->message_contents.epoch_ballot_preempted.requested_epoch_ballot = (struct epoch_ballot) {.epoch = eb_preempte->requested_epoch_ballot.epoch, .ballot = eb_preempte->requested_epoch_ballot.ballot};
    message_to_be_preempt->message_contents.epoch_ballot_preempted.acceptors_current_epoch_ballot = (struct epoch_ballot) {.epoch = eb_preempte->acceptors_current_epoch_ballot.epoch, .ballot = eb_preempte->acceptors_current_epoch_ballot.ballot};
}


void epoch_ballot_accept_from_paxos_accept(const struct paxos_accept* paxos_accept, uint32_t epoch, struct epoch_ballot_accept* converted_epoch_ballot){
    converted_epoch_ballot->instance = paxos_accept->iid;
    converted_epoch_ballot->epoch_ballot_requested = (struct epoch_ballot) {.epoch = epoch, .ballot = paxos_accept->ballot};
    copy_value(&paxos_accept->value, &converted_epoch_ballot->value_to_accept);
}

void union_epoch_ballot_chosen_from_epoch_ballot_accept(struct writeahead_epoch_paxos_message* chosen_message, const struct epoch_ballot_accept* accept){
    chosen_message->type = WRITEAHEAD_INSTANCE_CHOSEN_AT_EPOCH_BALLOT;
    chosen_message->message_contents.instance_chosen_at_epoch_ballot.instance = accept->instance;
    copy_epoch_ballot(&accept->epoch_ballot_requested, &chosen_message->message_contents.instance_chosen_at_epoch_ballot.chosen_epoch_ballot);
    copy_value(&accept->value_to_accept, &chosen_message->message_contents.instance_chosen_at_epoch_ballot.chosen_value);
}


void union_ballot_chosen_from_epoch_ballot_accept(struct standard_paxos_message* chosen_message, const struct paxos_accept* accept){
    chosen_message->type =PAXOS_CHOSEN;
    chosen_message->u.chosen.iid = accept->iid;
  //  chosen_message->u.chosen.ballot = accept->ballot;
  copy_ballot(&accept->ballot, &chosen_message->u.chosen.ballot);
    copy_value(&accept->value, &chosen_message->u.chosen.value);
}

void paxos_accepted_from_paxos_chosen(struct paxos_accepted* accepted, struct paxos_chosen* chosen){
   accepted->iid = chosen->iid;
   if (chosen->value.paxos_value_len != 0)
       copy_value(&chosen->value, &accepted->value);
//   accepted->ballot = chosen->ballot;
 //  accepted->value_ballot = chosen->ballot;
    copy_ballot(&chosen->ballot, &accepted->promise_ballot);
    copy_ballot(&chosen->ballot, &accepted->value_ballot);
   accepted->aid = 0;
}

void paxos_chosen_from_paxos_accepted(struct paxos_chosen* chosen, struct paxos_accepted* accepted) {
    chosen->iid = accepted->iid;
    //chosen->ballot = accepted->value_ballot;
    copy_ballot(&accepted->value_ballot, &chosen->ballot);
    copy_value(&accepted->value, &chosen->value);
}

void paxos_chosen_from_paxos_accept(struct paxos_chosen* chosen, struct paxos_accept* accept) {
    chosen->iid = accept->iid;
    copy_ballot(&accept->ballot, &chosen->ballot);
    copy_value(&accept->value, &chosen->value);
}

void paxos_accepted_from_paxos_prepare_and_accept(struct paxos_prepare* prepare, struct paxos_accept* accept, int id, struct paxos_accepted* accepted) {
    assert(prepare->iid == accept->iid);
    accepted->iid = prepare->iid;
    accepted->aid = id;
    copy_ballot(&prepare->ballot, &accepted->promise_ballot);
    copy_ballot(&accept->ballot, &accepted->value_ballot);
    copy_value(&accept->value, &accepted->value);
}

void
paxos_prepare_copy(struct paxos_prepare* dst, struct paxos_prepare* src){
   // dst->iid = src->iid;
  //  copy_ballot(&src->ballot, &dst->ballot);
    memcpy(dst, src, sizeof(struct paxos_prepare));
}


void
paxos_accepted_copy(struct paxos_accepted* dst, struct paxos_accepted* src) {
    memcpy(dst, src, sizeof(struct paxos_accepted));
    if (dst->value.paxos_value_len > 0) {
        dst->value.paxos_value_val = malloc(src->value.paxos_value_len);
        memcpy(dst->value.paxos_value_val, src->value.paxos_value_val,
               src->value.paxos_value_len);
    }
}

void
paxos_accept_copy(struct paxos_accept* dst, struct paxos_accept* src) {
    memcpy(dst, src, sizeof(struct paxos_accept));
    if (src->value.paxos_value_len > 0) {
        dst->value.paxos_value_val = calloc(src->value.paxos_value_len, sizeof(char));
        memcpy(dst->value.paxos_value_val, src->value.paxos_value_val, src->value.paxos_value_len);
    }
}



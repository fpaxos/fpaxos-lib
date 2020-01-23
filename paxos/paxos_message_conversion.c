//
// Created by Michael Davis on 11/12/2019.
//

//
// Created by Michael Davis on 11/12/2019.
//


#include "standard_acceptor.h"
#include "stable_storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <paxos_storage.h>
#include <paxos_types.h>


void copy_value(const struct paxos_value *value_to_copy, struct paxos_value *copied_value) {
    // assumes that copy
    char *value = NULL;
    int value_size = value_to_copy->paxos_value_len;
    if (value_size > 0) {
        value = calloc(1, value_size);
        memcpy(value, value_to_copy->paxos_value_val, value_size);
    }

    *copied_value = (struct paxos_value) {value_size, value};
}


void
paxos_accepted_to_promise(const struct paxos_accepted *acc, paxos_message *out) {
    out->type = PAXOS_PROMISE;
    out->u.promise = (paxos_promise) {
            acc->aid,
            acc->iid,
            acc->ballot,
            acc->value_ballot,
            {acc->value.paxos_value_len, acc->value.paxos_value_val}
    };
}

void
paxos_accept_to_accepted(int id, const struct paxos_accept *acc, paxos_message *out) {
    char *value = NULL;
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
}

void
paxos_accept_to_preempted(int id, const struct paxos_accept* accept, paxos_message *out){
    out->type = PAXOS_PREEMPTED;
    out->u.preempted = (paxos_preempted) {id, accept->iid, accept->ballot};
}
void
paxos_accepted_to_preempted(int id, const struct paxos_accepted *acc, paxos_message *out) {
    out->type = PAXOS_PREEMPTED;
    out->u.preempted = (paxos_preempted) {id, acc->iid, acc->ballot};
}

void
paxos_accepted_to_accept(const struct paxos_accepted *accepted, paxos_accept *out) {
    out->ballot = accepted->value_ballot;
    out->iid = accepted->iid;

    copy_value(&accepted->value, &out->value);

}
void
paxos_accepted_to_prepare(const struct paxos_accepted *accepted, paxos_prepare *out) {
    out->ballot = accepted->ballot;
    out->iid = accepted->iid;
}

void
paxos_promise_from_accept_and_prepare(const struct paxos_prepare* prepare, const struct paxos_accept* accept, const int aid, struct paxos_message* out){
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

void
paxos_prepare_copy(struct paxos_prepare* dst, struct paxos_prepare* src){
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
    if (dst->value.paxos_value_len > 0) {
        dst->value.paxos_value_val = malloc(src->value.paxos_value_len);
        memcpy(dst->value.paxos_value_val, src->value.paxos_value_val, src->value.paxos_value_len);
    }
}

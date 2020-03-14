//
// Created by Michael Davis on 11/03/2020.
//

#include <assert.h>
#include <paxos_types.h>
#include <paxos_storage.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "standard_stable_storage.h"
#include "standard_acceptor.h"
#include "ballot.h"


void copy_ballot(const struct ballot *src, struct ballot *dst) {
    *dst = (struct ballot) {.proposer_id = src->proposer_id, .number = src->number};
}

bool ballot_equal(const struct ballot *lhs, const struct ballot rhs) {
    return lhs->proposer_id == rhs.proposer_id && lhs->number == rhs.number;
}

bool ballot_greater_than(const struct ballot lhs, const struct ballot rhs) {
    if (lhs.number > rhs.number) {
        return true;
    } else {
        if (lhs.proposer_id > rhs.proposer_id && lhs.number == rhs.number) {
            return true;
        } else {
            return false;
        }
    }
}

bool ballot_greater_than_or_equal(const struct ballot lhs, const struct ballot rhs) {
    if (lhs.number > rhs.number) {
        return true;
    } else {
        if (lhs.proposer_id >= rhs.proposer_id && lhs.number == rhs.number) {
            return true;
        } else {
            return false;
        }
    }
}

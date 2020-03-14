//
// Created by Michael Davis on 11/03/2020.
//

#ifndef LIBPAXOS_BALLOT_H
#define LIBPAXOS_BALLOT_H

void copy_ballot(const struct ballot *src, struct ballot *dst);

bool ballot_equal(const struct ballot *lhs, const struct ballot rhs);

bool ballot_greater_than(const struct ballot lhs, const struct ballot rhs);

bool ballot_greater_than_or_equal(const struct ballot lhs, const struct ballot rhs);

#include <paxos_types.h>
#include <paxos.h>
#include "stdbool.h"

#endif //LIBPAXOS_BALLOT_H

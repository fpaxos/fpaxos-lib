//
// Created by Michael Davis on 11/03/2020.
//

#ifndef LIBPAXOS_PAXOS_VALUE_H
#define LIBPAXOS_PAXOS_VALUE_H

#include "stdbool.h"

struct paxos_value
{
	unsigned int paxos_value_len;
	char *paxos_value_val;
};

void copy_value(const struct paxos_value *src, struct paxos_value *dst);

void paxos_value_copy(struct paxos_value* dst, struct paxos_value* src);

bool is_values_equal(struct paxos_value lhs, struct paxos_value rhs);

//int paxos_value_cmp(struct paxos_value* v1, struct paxos_value* v2);

#endif //LIBPAXOS_PAXOS_VALUE_H

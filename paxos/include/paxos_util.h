//
// Created by Michael Davis on 08/02/2020.
//

#ifndef LIBPAXOS_PAXOS_UTIL_H
#define LIBPAXOS_PAXOS_UTIL_H

void carray_paxos_value_free(void* v);
int paxos_value_cmp(struct paxos_value* v1, struct paxos_value* v2);


#endif //LIBPAXOS_PAXOS_UTIL_H

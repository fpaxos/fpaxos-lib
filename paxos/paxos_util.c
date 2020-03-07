//
// Created by Michael Davis on 08/02/2020.
//



#include <paxos.h>
#include <string.h>

void
carray_paxos_value_free(void* v)
{
    paxos_value_free(v);
}


int
paxos_value_cmp(struct paxos_value* v1, struct paxos_value* v2)
{
    if (v1->paxos_value_len != v2->paxos_value_len)
        return -1;
    return memcmp(v1->paxos_value_val, v2->paxos_value_val, v1->paxos_value_len);
}

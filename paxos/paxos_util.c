//
// Created by Michael Davis on 08/02/2020.
//



#include <paxos.h>
#include <string.h>
#include "paxos_value.h"

void
carray_paxos_value_free(void* v)
{
    paxos_value_free(v);
}



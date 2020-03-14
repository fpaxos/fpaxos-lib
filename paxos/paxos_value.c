//
// Created by Michael Davis on 11/03/2020.
//

#include <assert.h>
#include <paxos_types.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "paxos_value.h"

void copy_value(const struct paxos_value *src, struct paxos_value *dst) {
    //assert(value_to_copy != NULL);
 //   assert(value_to_copy->paxos_value_val != NULL);
    // assumes that copy
    char *value = NULL;
    unsigned int value_size = src->paxos_value_len;
    if (value_size > 0) {
        value = calloc(1, value_size);
        memcpy(value, src->paxos_value_val, value_size);
    }

    *dst = (struct paxos_value) {value_size, value};
    if (src->paxos_value_len > 0) {
        assert(dst != NULL);
        assert(dst->paxos_value_val != NULL);
    }

    assert(is_values_equal(*src, *dst));
}

void
paxos_value_copy(struct paxos_value* dst, struct paxos_value* src)
{
    assert(src != NULL);
    assert(dst != NULL);
    unsigned int len = src->paxos_value_len;
    dst->paxos_value_len = len;
    if (src->paxos_value_len > 0) {
        dst->paxos_value_val = malloc(len);
        memcpy(dst->paxos_value_val, src->paxos_value_val, len);
    }
}

bool is_values_equal(struct paxos_value lhs, struct paxos_value rhs){
    if (lhs.paxos_value_len == rhs.paxos_value_len) {
        if (memcmp(lhs.paxos_value_val, rhs.paxos_value_val, lhs.paxos_value_len) == 0) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}
/*
int
paxos_value_cmp(struct paxos_value* v1, struct paxos_value* v2)
{
    if (v1->paxos_value_len != v2->paxos_value_len)
        return -1;
    return memcmp(v1->paxos_value_val, v2->paxos_value_val, v1->paxos_value_len);
}*/

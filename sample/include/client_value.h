//
// Created by Michael Davis on 11/03/2020.
//

#ifndef LIBPAXOS_CLIENT_VALUE_H
#define LIBPAXOS_CLIENT_VALUE_H

#include "paxos_value.h"
#include "stdio.h"
#include "sys/time.h"

struct client_value {
    int client_id;
    struct timeval submitted_at;
    size_t value_size;
    char* value;
};

void fill_paxos_value_from_client_value(struct client_value* src, struct paxos_value* dst);

void fill_client_value_from_paxos_value(struct paxos_value* src, struct client_value* dst);



#endif //LIBPAXOS_CLIENT_VALUE_H

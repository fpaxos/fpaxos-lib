//
// Created by Michael Davis on 07/02/2020.
//

#ifndef LIBPAXOS_INSTANCE_H
#define LIBPAXOS_INSTANCE_H


#include "paxos_types.h"
#include "paxos.h"
#include <sys/time.h>
#include "quorum.h"
#include "epoch_quorum.h"
#include "paxos_value.h"

struct proposer_common_instance_info {
    iid_t iid;
    struct ballot ballot;
    struct paxos_value* value_to_propose;
    struct paxos_value* last_promised_value;
    struct ballot value_ballot;
    struct timeval created_at;
};

struct standard_proposer_instance_info
{
    struct proposer_common_instance_info common_info;
    struct quorum quorum;
};

struct epoch_proposer_instance_info {
    struct proposer_common_instance_info common_info;
    struct epoch_quorum epoch_quorum;
};


#endif //LIBPAXOS_INSTANCE_H

//
// Created by Michael Davis on 08/02/2020.
//

#ifndef LIBPAXOS_PROPOSER_COMMON_H
#define LIBPAXOS_PROPOSER_COMMON_H


int
proposer_instance_info_has_value(struct proposer_common_instance_info *inst);

int
proposer_instance_info_has_promised_value(struct proposer_common_instance_info* inst);

int
proposer_instance_info_has_timedout(struct proposer_common_instance_info* inst, struct timeval* now);


//struct proposer_common_instance_info* proposer_common_instance_info_new(iid_t iid, ballot_t ballot, int acceptors, int q1);

struct standard_proposer_instance_info* standard_proposer_instance_info_new(iid_t iid, ballot_t ballot, int acceptors, int q1);
struct epoch_proposer_instance_info* epoch_propser_instance_info(iid_t iid, ballot_t ballot, int acceptors, int q1);

void proposer_instance_info_to_accept(struct proposer_common_instance_info* inst, paxos_accept* acc);

struct proposer_common_instance_info proposer_common_info_new(iid_t iid, struct ballot ballot);

void proposer_common_instance_info_free(struct proposer_common_instance_info* inst);

#endif //LIBPAXOS_PROPOSER_COMMON_H

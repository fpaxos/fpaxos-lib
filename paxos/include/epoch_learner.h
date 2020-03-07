//
// Created by Michael Davis on 07/02/2020.
//

#ifndef LIBPAXOS_EPOCH_LEARNER_H
#define LIBPAXOS_EPOCH_LEARNER_H


#include "paxos.h"

struct epoch_learner;

struct epoch_learner* epoch_learner_new(int acceptors);
void epoch_learner_epoch_ballot_chosen(struct epoch_learner* l, struct instance_chosen_at_epoch_ballot out_chosen);
void epoch_learner_receive_epoch_ballot_chosen(struct epoch_learner* l, struct instance_chosen_at_epoch_ballot* out);
void epoch_learner_free(struct epoch_learner* l);
void epoch_learner_set_instance_id(struct epoch_learner* l, iid_t iid);
void epoch_learner_receive_accepted(struct epoch_learner* l, struct epoch_ballot_accepted* ack);
int epoch_learner_deliver_next(struct epoch_learner* l, struct epoch_ballot_accepted* out);
int epoch_learner_has_holes(struct epoch_learner* l, iid_t* from, iid_t* to);


#endif //LIBPAXOS_EPOCH_LEARNER_H

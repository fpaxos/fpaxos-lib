//
// Created by Michael Davis on 07/02/2020.
//

#ifndef LIBPAXOS_EPOCH_PROPOSER_H
#define LIBPAXOS_EPOCH_PROPOSER_H

#include "paxos.h"

struct epoch_proposer;
struct timeout_iterator;

struct epoch_proposer* epoch_proposer_new(int id, int acceptors, int q1, int q2);
void epoch_proposer_free(struct epoch_proposer* p);
void epoch_proposer_propose(struct epoch_proposer* p, const char* value, size_t size);
int epoch_proposer_prepared_count(struct epoch_proposer* p);
void epoch_proposer_set_instance_id(struct epoch_proposer* p, iid_t iid);

// phase 1
void epoch_proposer_prepare(struct epoch_proposer* p, paxos_prepare* out);
void epoch_proposer_epoch_ballot_prepare(struct epoch_proposer* p, struct epoch_ballot_prepare* out);
int epoch_proposer_receive_epoch_ballot_promise(struct epoch_proposer* p, paxos_promise* ack,
                             paxos_prepare* out); // add to epoch_ballot_quorum


// phase 2
int epoch_proposer_epoch_ballot_accept(struct epoch_proposer* p, paxos_accept* out); //
int epoch_proposer_receive_epoch_ballot_accepted(struct epoch_proposer* p, paxos_accepted* ack); // add to quorum
void epoch_proposer_instance_chosen(struct epoch_proposer* p, struct instance_chosen_at_epoch_ballot* chosen); // increase instance and send to all acceptors and learners



// Out of dateness messages
int epoch_proposer_receive_preempted(struct epoch_proposer* p, paxos_preempted* ack,
                               paxos_prepare* out); // timeout ++ and ballot increase + acceptor current or epoch
int epoch_proposer_receive_instance_chosen(struct epoch_proposer* p, struct instance_chosen_at_epoch_ballot* chosen); // set current instance ++



// periodic acceptor state
void epoch_proposer_receive_acceptor_state(struct epoch_proposer* p,
                                     paxos_standard_acceptor_state* state);

// timeouts
struct timeout_iterator* epoch_proposer_timeout_iterator(struct epoch_proposer* p);
int timeout_iterator_prepare(struct timeout_iterator* iter, paxos_prepare* out);
int timeout_iterator_accept(struct timeout_iterator* iter, paxos_accept* out);
void timeout_iterator_free(struct timeout_iterator* iter);

#endif //LIBPAXOS_EPOCH_PROPOSER_H

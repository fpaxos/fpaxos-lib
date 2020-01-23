//
// Created by Michael Davis on 11/12/2019.
//

#ifndef LIBPAXOS_PAXOS_MESSAGE_CONVERSION_H
#define LIBPAXOS_PAXOS_MESSAGE_CONVERSION_H


void paxos_accepted_to_promise(const struct paxos_accepted *acc, paxos_message *out);

void paxos_accept_to_accepted(int id, const struct paxos_accept *acc, paxos_message *out);

void paxos_accepted_to_preempted(int id, const struct paxos_accepted *acc, paxos_message *out);

void paxos_accepted_to_accept(const struct paxos_accepted *accepted, paxos_accept *out);

void paxos_accepted_to_prepare(const struct paxos_accepted *accepted, paxos_prepare *out);

void paxos_accept_to_preempted(int id, const struct paxos_accept* accept, paxos_message *out);

void paxos_promise_from_accept_and_prepare(const struct paxos_prepare* prepare, const struct paxos_accept* accept, const int aid, struct paxos_message* out);


void paxos_accepted_copy(paxos_accepted* dst, paxos_accepted* src);

void paxos_prepare_copy(struct paxos_prepare* dst, struct paxos_prepare* src);

void paxos_accept_copy(struct paxos_accept* dst, struct paxos_accept* src);


#endif //LIBPAXOS_PAXOS_MESSAGE_CONVERSION_H

//
// Created by Michael Davis on 11/12/2019.
//

#ifndef LIBPAXOS_PAXOS_MESSAGE_CONVERSION_H
#define LIBPAXOS_PAXOS_MESSAGE_CONVERSION_H

#include <paxos_types.h>
#include "stdbool.h"
#include <paxos.h>

bool ballot_greater_than(const struct ballot lhs, const struct ballot rhs);

bool ballot_greater_than_or_equal(const struct ballot lhs, const struct ballot rhs);

bool ballot_equal(const struct ballot *lhs, const struct ballot rhs);

void copy_ballot(const struct ballot *src, struct ballot *dst);

void copy_value(const struct paxos_value *value_to_copy, struct paxos_value *copied_value);

void paxos_accepted_to_promise(const struct paxos_accepted *acc, standard_paxos_message *out);

void paxos_accept_to_accepted(int id, const struct paxos_accept *acc, standard_paxos_message *out);

void paxos_accepted_to_preempted(int id, const struct paxos_accepted *acc, standard_paxos_message *out);

void union_paxos_prepare_to_preempted(int id, const struct paxos_prepare *prepare, struct standard_paxos_message *out);

void paxos_accepted_to_accept(const struct paxos_accepted *accepted, paxos_accept *out);

void paxos_accepted_to_prepare(const struct paxos_accepted *accepted, paxos_prepare *out);

void paxos_accept_to_preempted(int id, const struct paxos_accept* accept, standard_paxos_message *out);

void paxos_prepare_from_epoch_ballot_prepare(const struct epoch_ballot_prepare* epoch_ballot_prepare, struct paxos_prepare* out); // information lost

void union_paxos_promise_from_accept_and_prepare(const struct paxos_prepare* prepare, const struct paxos_accept* accept, const int aid, struct standard_paxos_message* out);

void epoch_ballot_accept_from_paxos_accepted(const struct paxos_accepted* accepted, uint32_t accepted_epoch, struct epoch_ballot_accept* epoch_ballot_accept);

void paxos_accepted_from_epoch_ballot_accept(const struct epoch_ballot_accept* epoch_ballot_accept, int aid, struct paxos_accepted* accepted);

void paxos_accept_from_epoch_ballot_accept(const struct epoch_ballot_accept* eb_accept, struct paxos_accept* converted_accept);

void epoch_ballot_accept_from_paxos_accept(const struct paxos_accept* paxos_accept, uint32_t epoch, struct epoch_ballot_accept* converted_epoch_ballot);

void epoch_ballot_preempted_from_epoch_ballot_requested_and_epoch_ballot_last_responded(int aid, const iid_t instance, const struct epoch_ballot* requested_epoch_ballot, const struct epoch_ballot* last_responded_epoch_ballot, struct epoch_ballot_preempted* preempted);

void union_epoch_ballot_preempted_from_epoch_ballot_preempted(const struct epoch_ballot_preempted* eb_preempte, struct writeahead_epoch_paxos_message* message_to_be_preempt);

void union_epoch_ballot_promise_from_epoch_ballot_accept_and_epoch_ballot_prepare(struct writeahead_epoch_paxos_message* message_to_be_promise, const struct epoch_ballot_prepare* prepare, const struct epoch_ballot_accept* accept, int aid);

void union_epoch_ballot_accepted_from_epoch_ballot_accept(struct writeahead_epoch_paxos_message* message_to_be_accepted, const struct epoch_ballot_accept *accept, int acceptor_id);

void union_epoch_ballot_promise_from_epoch_ballot_accept_and_paxos_prepare(struct writeahead_epoch_paxos_message* message_to_be_promise, const struct paxos_prepare* prepare, const struct epoch_ballot_accept* accept, int aid, uint32_t promised_epoch);

void union_epoch_ballot_chosen_from_epoch_ballot_accept(struct writeahead_epoch_paxos_message* chosen_message, const struct epoch_ballot_accept* accept);

void union_ballot_chosen_from_epoch_ballot_accept(struct standard_paxos_message* chosen_message, const struct paxos_accept* accept);

void paxos_accepted_from_paxos_chosen(struct paxos_accepted* accepted, struct paxos_chosen* chosen);

void paxos_chosen_from_paxos_accepted(struct paxos_chosen* chosen, struct paxos_accepted* accepted);

void paxos_chosen_from_paxos_accept(struct paxos_chosen* chosen, struct paxos_accept* accept);

void paxos_accepted_from_paxos_prepare_and_accept(struct paxos_prepare* prepare, struct paxos_accept* accept, int id, struct paxos_accepted* accepted);

void paxos_accepted_copy(paxos_accepted* dst, paxos_accepted* src);

void paxos_prepare_copy(struct paxos_prepare* dst, struct paxos_prepare* src);

void paxos_accept_copy(struct paxos_accept* dst, struct paxos_accept* src);

void
paxos_value_copy(paxos_value* dst, paxos_value* src);

#endif //LIBPAXOS_PAXOS_MESSAGE_CONVERSION_H

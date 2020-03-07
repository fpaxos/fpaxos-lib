//
// Created by Michael Davis on 03/02/2020.
//

#include "paxos.h"
#include "paxos_types.h"

#ifndef LIBPAXOS_WRITEAHEAD_EPOCH_ACCEPTOR_H
#define LIBPAXOS_WRITEAHEAD_EPOCH_ACCEPTOR_H


struct writeahead_epoch_acceptor;

struct writeahead_epoch_acceptor* writeahead_epoch_acceptor_init(int id);

void writeahead_epoch_acceptor_free(struct writeahead_epoch_acceptor* acceptor);

int writeahead_epoch_acceptor_receive_prepare(struct writeahead_epoch_acceptor* acceptor, struct paxos_prepare* request, struct writeahead_epoch_paxos_message* returned_message);

int writeahead_epoch_acceptor_receive_epoch_ballot_prepare(struct writeahead_epoch_acceptor* acceptor, struct epoch_ballot_prepare* request, struct writeahead_epoch_paxos_message* returned_message);

int writeahead_epoch_acceptor_receive_epoch_ballot_accept(struct writeahead_epoch_acceptor* acceptor, struct epoch_ballot_accept* request, struct writeahead_epoch_paxos_message* response);

int  writeahead_epoch_acceptor_receive_repeat(struct writeahead_epoch_acceptor* acceptor, iid_t iid, struct writeahead_epoch_paxos_message* response); //todo

int  writeahead_epoch_acceptor_receive_trim(struct writeahead_epoch_acceptor* acceptor, struct paxos_trim* trim); //todo

int  writeahead_epoch_acceptor_receive_epoch_notification(struct writeahead_epoch_acceptor* acceptor, struct epoch_notification* epoch_notification);

int writeahead_epoch_acceptor_receive_instance_chosen(struct writeahead_epoch_acceptor* acceptor, struct instance_chosen_at_epoch_ballot); // todo

#endif //LIBPAXOS_WRITEAHEAD_EPOCH_ACCEPTOR_H

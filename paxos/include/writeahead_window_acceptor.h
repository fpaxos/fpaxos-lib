//
// Created by Michael Davis on 18/12/2019.
//

#ifndef LIBPAXOS_WRITEAHEAD_WINDOW_ACCEPTOR_H
#define LIBPAXOS_WRITEAHEAD_WINDOW_ACCEPTOR_H



#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "paxos.h"
#include "standard_stable_storage.h"

//struct writeahead_window_acceptor;

/*struct writeahead_window_acceptor *
write_ahead_window_acceptor_new (
        int id,
        int min_instance_catchup,
        int min_ballot_catchup,
        int ballot_window,
        int instance_window
);*/


struct writeahead_window_acceptor;

struct writeahead_window_acceptor* write_ahead_window_acceptor_new(int id, int min_instance_catchup, int min_ballot_catchup, int bal_window, int instance_window, int instance_epoch_writing_iteration_size);

iid_t write_ahead_ballot_acceptor_get_trim(struct writeahead_window_acceptor* acceptor);

void write_ahead_window_acceptor_free(struct writeahead_window_acceptor *a);

int write_ahead_window_acceptor_receive_prepare(struct writeahead_window_acceptor *a,
                                                paxos_prepare *req, standard_paxos_message *out);

int write_ahead_window_acceptor_receive_accept(struct writeahead_window_acceptor *a,
                                               paxos_accept *req, standard_paxos_message *out);

int write_ahead_window_acceptor_receive_repeat(struct writeahead_window_acceptor *a,
                                     iid_t iid, struct standard_paxos_message *out);

int write_ahead_window_acceptor_receive_trim(struct writeahead_window_acceptor *a, paxos_trim *trim);

void write_ahead_window_acceptor_get_current_state(struct writeahead_window_acceptor *a, paxos_standard_acceptor_state *state);

void write_ahead_window_acceptor_check_and_update_write_ahead_windows(struct writeahead_window_acceptor* acceptor);

bool write_ahead_acceptor_is_new_instance_epoch_needed(struct writeahead_window_acceptor* acceptor);

void write_ahead_acceptor_write_iteration_of_instance_epoch(struct writeahead_window_acceptor* acceptor);

void write_ahead_acceptor_check_and_update_ballot_epochs(struct writeahead_window_acceptor* acceptor);

bool write_ahead_acceptor_is_writing_epoch(struct writeahead_window_acceptor* acceptor);

void write_ahead_acceptor_begin_writing_instance_epoch(struct writeahead_window_acceptor* acceptor);

void write_ahead_acceptor_clean_up_instance_epoch_stuff(struct writeahead_window_acceptor* acceptor);


int write_ahead_ballot_acceptor_receive_chosen(struct writeahead_window_acceptor* a, struct paxos_chosen *chosen);


bool write_ahead_acceptor_check_ballot_window(struct writeahead_window_acceptor* acceptor);

void write_ahead_acceptor_write_ballot_window(struct writeahead_window_acceptor* acceptor);

#ifdef __cplusplus
}
#endif



#endif //LIBPAXOS_WRITEAHEAD_WINDOW_ACCEPTOR_H

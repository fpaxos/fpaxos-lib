//
// Created by Michael Davis on 18/12/2019.
//

#ifndef LIBPAXOS_WRITE_AHEAD_WINDOW_ACCEPTOR_H
#define LIBPAXOS_WRITE_AHEAD_WINDOW_ACCEPTOR_H



#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "paxos.h"
#include "stable_storage.h"

//struct write_ahead_window_acceptor;

/*struct write_ahead_window_acceptor *
write_ahead_window_acceptor_new (
        int id,
        int min_instance_catchup,
        int min_ballot_catchup,
        int ballot_window,
        int instance_window
);*/


struct write_ahead_window_acceptor;

struct write_ahead_window_acceptor* write_ahead_window_acceptor_new(int id, int min_instance_catchup, int min_ballot_catchup, int bal_window, int instance_window, int instance_epoch_writing_iteration_size);

void write_ahead_window_acceptor_free(struct write_ahead_window_acceptor *a);

int write_ahead_window_acceptor_receive_prepare(struct write_ahead_window_acceptor *a,
                                      paxos_prepare *req, paxos_message *out);

int write_ahead_window_acceptor_receive_accept(struct write_ahead_window_acceptor *a,
                                     paxos_accept *req, paxos_message *out);

int write_ahead_window_acceptor_receive_repeat(struct write_ahead_window_acceptor *a,
                                     iid_t iid, paxos_accepted *out);

int write_ahead_window_acceptor_receive_trim(struct write_ahead_window_acceptor *a, paxos_trim *trim);

void write_ahead_window_acceptor_set_current_state(struct write_ahead_window_acceptor *a, paxos_standard_acceptor_state *state);

void write_ahead_window_acceptor_check_and_update_write_ahead_windows(struct write_ahead_window_acceptor* acceptor);

bool write_ahead_acceptor_is_new_instance_epoch_needed(struct write_ahead_window_acceptor* acceptor);

void write_ahead_acceptor_write_iteration_of_instance_epoch(struct write_ahead_window_acceptor* acceptor);

void write_ahead_acceptor_check_and_update_ballot_epochs(struct write_ahead_window_acceptor* acceptor);

bool write_ahead_acceptor_is_writing_epoch(struct write_ahead_window_acceptor* acceptor);

void write_ahead_acceptor_begin_writing_instance_epoch(struct write_ahead_window_acceptor* acceptor);

void write_ahead_acceptor_clean_up_instance_epoch_stuff(struct write_ahead_window_acceptor* acceptor);
#ifdef __cplusplus
}
#endif



#endif //LIBPAXOS_WRITE_AHEAD_WINDOW_ACCEPTOR_H

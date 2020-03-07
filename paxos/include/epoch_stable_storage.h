//
// Created by Michael Davis on 03/02/2020.
//

#ifndef LIBPAXOS_EPOCH_STABLE_STORAGE_H
#define LIBPAXOS_EPOCH_STABLE_STORAGE_H

#include "standard_stable_storage.h"


struct epoch_stable_storage {
    void* extended_handle;
    struct {
        int (*store_current_epoch)(void *handle, const uint32_t epoch);
        int (*get_current_epoch)(void *handle, uint32_t* retrieved_epoch);
        int (*store_accept_epoch)(void* handle, const iid_t instance, const uint32_t accept_epoch);
        int (*get_accept_epoch)(void* handle, const iid_t instance, uint32_t* retrieved_accept_epoch);
    } extended_api ;
    struct standard_stable_storage standard_storage;
};

int epoch_stable_storage_open(struct epoch_stable_storage *stable_storage);

void epoch_stable_storage_close(struct epoch_stable_storage *store);

int epoch_stable_storage_tx_begin(struct epoch_stable_storage *store);

int epoch_stable_storage_tx_commit(struct epoch_stable_storage *store);

void epoch_stable_storage_tx_abort(struct epoch_stable_storage *store);

int epoch_stable_storage_store_trim_instance(struct epoch_stable_storage *stable_storage, const iid_t iid);

int epoch_stable_storage_get_trim_instance(struct epoch_stable_storage *store, iid_t * instance_id);

int epoch_stable_storage_get_epoch_ballot_accept(struct epoch_stable_storage *store, const iid_t instance_id, struct epoch_ballot_accept *retrieved_epoch_ballot);

int epoch_stable_storage_store_epoch_ballot_accept(struct epoch_stable_storage *store, struct epoch_ballot_accept* epoch_ballot_accept);

int epoch_stable_storage_get_all_untrimmed_epoch_ballot_accepts(struct epoch_stable_storage *store, struct epoch_ballot_accept **retrieved_epoch_ballot_accepts,
                                                                int *number_of_instances_retrieved);

int epoch_stable_storage_get_max_inited_instance(struct epoch_stable_storage *storage, iid_t *retrieved_instance);


int epoch_stable_storage_get_current_epoch(struct epoch_stable_storage* storage, uint32_t* retrieved_epoch);
int epoch_stable_storage_store_epoch(struct epoch_stable_storage* storage, const uint32_t epoch);

void epoch_stable_storage_init(struct epoch_stable_storage* storage, int acceptor_id);
void epoch_stable_storage_lmdb_init(struct epoch_stable_storage* storage, int acceptor_id);

#endif //LIBPAXOS_EPOCH_STABLE_STORAGE_Hw

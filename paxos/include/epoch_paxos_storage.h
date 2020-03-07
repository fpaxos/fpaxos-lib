//
// Created by Michael Davis on 04/02/2020.
//

#ifndef LIBPAXOS_EPOCH_PAXOS_STORAGE_H
#define LIBPAXOS_EPOCH_PAXOS_STORAGE_H

#include "paxos.h"
#include "paxos_storage.h"

struct epoch_paxos_storage {
    void* extended_handle;
    struct {
        int (*get_accept_epoch)(void* paxos_storage, iid_t instance, uint32_t* retreived_epoch);
        int (*store_accept_epoch)(void* paxos_storage, iid_t instance, uint32_t epoch);

    } extended_api;
    struct paxos_storage paxos_storage;
};

int epoch_paxos_storage_init(struct epoch_paxos_storage* store, int aid);

void epoch_paxos_storage_init_with_prepares_and_accepts(struct epoch_paxos_storage *epoch_paxos_storage , struct paxos_prepare** prepares, int number_of_prepares, struct epoch_ballot_accept** accepts, int number_of_accepts);

// FOR HASH_MAPPED_MEMORY
void epoch_hash_mapped_memory_init(struct epoch_paxos_storage* store, int aid);

void epoch_paxos_storage_init_with_prepares_and_accepts(struct epoch_paxos_storage *epoch_paxos_storage , struct paxos_prepare** prepares, int number_of_prepares, struct epoch_ballot_accept** accepts, int number_of_accepts);


// EPOCH METHODS
//int epoch_paxos_storage_get_instance_last_accepted_epoch(struct epoch_paxos_storage* storage, const iid_t instance, uint32_t* retreived_)

// WRAPPERS FOR PAXOS_STORAGE METHODS


int epoch_paxos_storage_get_max_inited_instance(struct epoch_paxos_storage* epoch_paxos_storage, iid_t* returned_max_inited_instance);

int epoch_paxos_storage_store_last_prepare(struct epoch_paxos_storage* paxos_storage, const struct paxos_prepare *last_prepare);

int epoch_paxos_storage_get_last_prepare(struct epoch_paxos_storage* paxos_storage, iid_t instance_id, paxos_prepare *last_promise_retrieved);

int epoch_paxos_storage_store_last_prepares(struct epoch_paxos_storage* paxos_storage, paxos_prepare **last_promises, int number_of_instances);

int epoch_paxos_storage_get_last_prepares(struct epoch_paxos_storage* paxos_storage, iid_t *instance_ids, int number_of_instances_to_retrieve,
                      paxos_prepare **last_promises_retrieved, int *number_of_instances_retrieved);

int epoch_paxos_storage_store_accept(struct epoch_paxos_storage *paxos_storage, struct epoch_ballot_accept *epoch_ballot_accept);

int epoch_paxos_storage_get_last_accept(struct epoch_paxos_storage *epoch_paxos_storage, iid_t instance_id, struct epoch_ballot_accept *last_accepted_retrieved);

int epoch_paxos_storage_store_accepts(struct epoch_paxos_storage *paxos_storage, struct epoch_ballot_accept **last_accepteds, int number_of_instances);

int epoch_paxos_storage_get_accepts(struct epoch_paxos_storage *volatile_storage, iid_t *instance_ids, int number_of_instances_to_retrieve,
                                    struct epoch_ballot_accept **last_accepteds_retrieved);

int epoch_paxos_storage_store_trim_instance(struct epoch_paxos_storage *volatile_storage, iid_t trim_instance_id);

int epoch_paxos_storage_get_trim_instance(struct epoch_paxos_storage* paxos_storage, iid_t *trim_instance_id_retrieved);




int epoch_paxos_storage_is_instance_chosen(struct epoch_paxos_storage* ep_storage, iid_t instance, bool* is_chosen);

int epoch_paxos_storage_set_instance_chosen(struct epoch_paxos_storage* ep_storage, iid_t instance);


//int epoch_paxos_storage_get_all_untrimmed_instances_info(struct epoch_paxos_storage *paxos_storage, paxos_accepted **retrieved_instances_info,
      //                               int *number_of_instances_retrieved);

//int epoch_paxos_storage_store_instance_info(struct epoch_paxos_storage* paxos_storage, const struct epoch_ballot_accepted* instance_info);

//int epoch_paxos_storage_get_instance_info(struct epoch_paxos_storage *paxos_storage, iid_t instance_id, struct epoch_ballot_accepted *instance_info_retrieved);

#endif //LIBPAXOS_EPOCH_PAXOS_STORAGE_H

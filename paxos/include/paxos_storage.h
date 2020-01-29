//
// Created by Michael Davis on 13/11/2019.
//

#ifndef LIBPAXOS_VOLATILE_STORAGE_H
#define LIBPAXOS_VOLATILE_STORAGE_H

#include "paxos_types.h"
#include "paxos.h"

#ifdef __cplusplus
extern "C" {
#endif


struct paxos_storage {
    void *handle;

    struct {
        // paxos_prepare used instead of promise and the promise contains unnecessary information
        int (*store_last_promise)(void *volatile_storage, const struct paxos_prepare *last_promise);

        int (*get_last_promise)(void *volatile_storage, iid_t instance_id, paxos_prepare *last_promise_retrieved);

        int (*store_last_promises)(void *volatile_storage, paxos_prepare **last_promises, int number_of_instances);

        int (*get_last_promises)(void *volatile_storage, iid_t *instances, int number_of_instances_to_retrieve,
                                 paxos_prepare **last_promises_retrieved, int *number_of_instances_retrieved);
        // int (*get_all_untrimmed_instances_promises)(void* paxos_storage, paxos_prepare** promises_retrieved, int* number_of_instances_retrieved);

        // Similar to paxos_prepare, paxos_accept is used here as it contains only the necessary information for safety
        int (*store_last_accepted)(void *volatile_storage, paxos_accept *accepted);

        int (*get_last_accepted)(void *volatile_storage, iid_t instance_id, paxos_accept *last_accepted_retrieved);

        int (*store_last_accepteds)(void *volatile_storage, paxos_accept **last_accepteds,
                                    int number_of_instances_to_store);

        int (*get_last_accepteds)(void *volatile_storage, iid_t *instances, int number_of_instances_to_retrieve,
                                  paxos_accept **last_accepteds_retrieved);
        //      int (*get_all_untrimmed_instances_accepteds)(void* paxos_storage, paxos_accept** last_accepteds_retrieved, int number_of_instances);

        int (*store_trim_instance)(void *volatile_storage, iid_t trim_instance_id);

        int (*get_trim_instance)(void *volatile_storage, iid_t *trim_instance_id_retrieved);

        int (*store_instance_info)(void *paxos_storage, const struct paxos_accepted *instance_info_to_store);

        int (*get_instance_info)(void *paxos_storage, iid_t instance_id, paxos_accepted *instance_info_retrieved);

        int (*get_all_untrimmed_instances_info)(void *paxos_storage, paxos_accepted **retrieved_instances_info,
                                                int *number_of_instances_retrieved);

        int (*get_max_inited_instance)(void *paxos_storage, iid_t* returned_max_inited_instance);
    } api;
};

int get_max_inited_instance(const struct paxos_storage* paxos_storage, iid_t* returned_max_inited_instance);

void init_paxos_storage(struct paxos_storage* paxos_storage);

void init_paxos_storage_with_instances_info(struct paxos_storage* paxos_storage, struct paxos_accepted** instances_info, int number_of_instances, iid_t trim_id);

int store_last_prepare(struct paxos_storage *paxos_storage, const struct paxos_prepare *last_prepare);

int get_last_promise(struct paxos_storage *paxos_storage, iid_t instance_id, paxos_prepare *last_promise_retrieved);

int store_last_promises(struct paxos_storage *paxos_storage, paxos_prepare **last_promises, int number_of_instances);

int get_last_promises(struct paxos_storage *paxos_storage, iid_t *instance_ids, int number_of_instances_to_retrieve,
                      paxos_prepare **last_promises_retrieved, int *number_of_instances_retrieved);

int store_acceptance(struct paxos_storage *paxos_storage, paxos_accept *accepted_ballot);

int get_last_accept(struct paxos_storage *volatile_storage, iid_t instance_id, paxos_accept *last_accepted_retrieved);

int store_acceptances(struct paxos_storage *paxos_storage, paxos_accept **last_accepteds, int number_of_instances);

int get_last_accepteds(struct paxos_storage *volatile_storage, iid_t *instance_ids, int number_of_instances_to_retrieve,
                       paxos_accept **last_accepteds_retrieved);

int store_trim_instance(struct paxos_storage *volatile_storage, iid_t trim_instance_id);

int get_trim_instance(struct paxos_storage *paxos_storage, iid_t *trim_instance_id_retrieved);

//int get_instance_info(struct paxos_storage* volatile_storage, iid_t instance_id, paxos_accepted* instance_info_retrieved);
//int store_instance_info(struct paxos_storage* paxos_storage, paxos_accepted* instance_info_to_store);

int get_all_untrimmed_instances_info(struct paxos_storage *paxos_storage, paxos_accepted **retrieved_instances_info,
                                     int *number_of_instances_retrieved);

int store_instance_info(struct paxos_storage* paxos_storage, const struct paxos_accepted* instance_info);

int get_instance_info(struct paxos_storage *paxos_storage, iid_t instance_id, paxos_accepted *instance_info_retrieved);


#ifdef __cplusplus
}
#endif

#endif


//LIBPAXOS_VOLATILE_STORAGE_H


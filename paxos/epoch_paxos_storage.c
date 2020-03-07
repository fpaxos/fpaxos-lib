//
// Created by Michael Davis on 04/02/2020.
//



#include <paxos.h>
#include <paxos_types.h>
#include <epoch_paxos_storage.h>
#include <paxos_message_conversion.h>
#include <string.h>
#include <paxos_storage.h>

int epoch_paxos_storage_init(struct epoch_paxos_storage* store, int aid){
    epoch_hash_mapped_memory_init(store, aid);
    return 0;
}

void epoch_paxos_storage_init_with_prepares_and_accepts(struct epoch_paxos_storage *epoch_paxos_storage , struct paxos_prepare** prepares, int number_of_prepares, struct epoch_ballot_accept** accepts, int number_of_accepts){
    epoch_hash_mapped_memory_init(epoch_paxos_storage, NULL);

    // store all the prepares
    epoch_paxos_storage_store_last_prepares(epoch_paxos_storage, prepares, number_of_prepares);

    // lazzzzy way
    for (int i = 0; i < number_of_accepts; i++){
        struct paxos_accept current_instance_as_accept;
        memset(&current_instance_as_accept, 0, sizeof(struct paxos_accept));
        // store all the accepts
        paxos_accept_from_epoch_ballot_accept(accepts[i], &current_instance_as_accept);
        // store all the epochs of accepts
        epoch_paxos_storage->extended_api.store_accept_epoch(epoch_paxos_storage->paxos_storage.handle, accepts[i]->instance, accepts[i]->epoch_ballot_requested.epoch);
    }
}

// EPOCH METHODS
//int epoch_paxos_storage_get_instance_last_accepted_epoch(struct epoch_paxos_storage* storage, const iid_t instance, uint32_t* retreived_)

// WRAPPERS FOR PAXOS_STORAGE METHODS


int epoch_paxos_storage_get_max_inited_instance(struct epoch_paxos_storage* epoch_paxos_storage, iid_t* returned_max_inited_instance){
    return epoch_paxos_storage->paxos_storage.api.get_max_inited_instance(epoch_paxos_storage->paxos_storage.handle, returned_max_inited_instance);
}

int epoch_paxos_storage_store_last_prepare(struct epoch_paxos_storage* paxos_storage, const struct paxos_prepare *last_prepare){
    return paxos_storage->paxos_storage.api.store_last_promise(paxos_storage->paxos_storage.handle, last_prepare);
}

int epoch_paxos_storage_get_last_prepare(struct epoch_paxos_storage* paxos_storage, iid_t instance_id, paxos_prepare *last_promise_retrieved){
    return paxos_storage->paxos_storage.api.get_last_promise(paxos_storage->paxos_storage.handle, instance_id, last_promise_retrieved);
}

int epoch_paxos_storage_store_last_prepares(struct epoch_paxos_storage* paxos_storage, paxos_prepare **last_promises, int number_of_instances){
    return paxos_storage->paxos_storage.api.store_last_promises(paxos_storage, last_promises, number_of_instances);
}

int epoch_paxos_storage_get_last_prepares(struct epoch_paxos_storage* paxos_storage, iid_t *instance_ids, int number_of_instances_to_retrieve,
                                          paxos_prepare **last_promises_retrieved, int *number_of_instances_retrieved){
    return paxos_storage->paxos_storage.api.get_last_promises(paxos_storage->paxos_storage.handle, instance_ids, number_of_instances_to_retrieve, last_promises_retrieved, number_of_instances_retrieved);
}


// HANDY METHODS FOR STORING EPOCH BALLOT ACCEPTS EASILY
void lazy_store_of_epoch_ballot_accept(const struct epoch_paxos_storage *paxos_storage,
                                      const struct epoch_ballot_accept *epoch_ballot_accept) {
    struct paxos_accept converted_accept;
    paxos_accept_from_epoch_ballot_accept(epoch_ballot_accept, &converted_accept);
    paxos_storage->extended_api.store_accept_epoch(paxos_storage->extended_handle, epoch_ballot_accept->instance, epoch_ballot_accept->epoch_ballot_requested.epoch);
    paxos_storage->paxos_storage.api.store_last_accepted(paxos_storage->paxos_storage.handle, &converted_accept);
}


int lazy_get_of_epoch_ballot_accept(const struct epoch_paxos_storage *epoch_paxos_storage, iid_t instance_id,
                                     struct epoch_ballot_accept *last_accepted_retrieved) {
    struct paxos_accept retrieved_paxos_accept;
    epoch_paxos_storage->paxos_storage.api.get_last_accepted(epoch_paxos_storage->paxos_storage.handle, instance_id, &retrieved_paxos_accept);
    // then get the epoch of that accept
    uint32_t epoch;
    int found = epoch_paxos_storage->extended_api.get_accept_epoch(epoch_paxos_storage->extended_handle, instance_id, &epoch);
    // then convert to an epoch_ballot_accept
    epoch_ballot_accept_from_paxos_accept(&retrieved_paxos_accept, epoch, last_accepted_retrieved);
    return found;
}


int epoch_paxos_storage_store_accept(struct epoch_paxos_storage *paxos_storage, struct epoch_ballot_accept *epoch_ballot_accept){
    lazy_store_of_epoch_ballot_accept(paxos_storage, epoch_ballot_accept);
    return 1;
}


int epoch_paxos_storage_get_last_accept(struct epoch_paxos_storage *epoch_paxos_storage, iid_t instance_id, struct epoch_ballot_accept *last_accepted_retrieved){
    // need to get the paxos_accept
    lazy_get_of_epoch_ballot_accept(epoch_paxos_storage, instance_id, last_accepted_retrieved);
    return 1;// lazy again ;)
}


int epoch_paxos_storage_store_accepts(struct epoch_paxos_storage *paxos_storage, struct epoch_ballot_accept **last_accepteds, int number_of_instances){
    for (int i = 0; i < number_of_instances; i++) {
        lazy_store_of_epoch_ballot_accept(paxos_storage, last_accepteds[i]);
    }
    return 1;
}

int epoch_paxos_storage_get_accepts(struct epoch_paxos_storage *volatile_storage, iid_t *instance_ids, int number_of_instances_to_retrieve,
                                    struct epoch_ballot_accept **last_accepteds_retrieved){
    int all_found = 1;
    for (int i = 0; i < number_of_instances_to_retrieve; i++){
        int found = lazy_get_of_epoch_ballot_accept(volatile_storage, instance_ids[i], last_accepteds_retrieved[i]);
        if (!found)
            all_found = 0;
    }
    return all_found;
}

int epoch_paxos_storage_store_trim_instance(struct epoch_paxos_storage *volatile_storage, iid_t trim_instance_id){
    return volatile_storage->paxos_storage.api.store_trim_instance(volatile_storage->paxos_storage.handle, trim_instance_id);
}

int epoch_paxos_storage_get_trim_instance(struct epoch_paxos_storage* paxos_storage, iid_t *trim_instance_id_retrieved){
    return paxos_storage->paxos_storage.api.get_trim_instance(paxos_storage->paxos_storage.handle, trim_instance_id_retrieved);
}


int epoch_paxos_storage_is_instance_chosen(struct epoch_paxos_storage* ep_storage, iid_t instance, bool* is_chosen){
    return ep_storage->paxos_storage.api.is_instance_chosen(ep_storage->paxos_storage.handle, instance, is_chosen);
}

int epoch_paxos_storage_set_instance_chosen(struct epoch_paxos_storage* ep_storage, iid_t instance) {
    return ep_storage->paxos_storage.api.set_instance_chosen(ep_storage->paxos_storage.handle, instance);
}


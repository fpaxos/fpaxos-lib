/*
 * Copyright (c) 2013-2015, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "standard_stable_storage.h"
#include <stdlib.h>
#include <standard_stable_storage.h>
#include "paxos_storage.h"

void
storage_init(struct standard_stable_storage *stable_storage, int acceptor_id) {
 //   switch (paxos_config.storage_backend) {
       // case PAXOS_MEM_STORAGE:
        //    storage_init_mem(stable_storage, acceptor_id);
        //    break;
//#ifdef HAS_LMDB
        //case PAXOS_LMDB_STORAGE:
        storage_init_lmdb(stable_storage, acceptor_id);
         //   break;
//#endif
     //   default:
     //       paxos_log_error("Storage backend not available");
     //       exit(0);
   // }
}

int
storage_open(struct standard_stable_storage *stable_storage) {
    return stable_storage->api.open(stable_storage->handle);
}

void
storage_close(struct standard_stable_storage *stable_storage) {
    stable_storage->api.close(stable_storage->handle);
}

int
storage_tx_begin(struct standard_stable_storage *stable_storage) {
    return stable_storage->api.tx_begin(stable_storage->handle);
}

int
storage_tx_commit(struct standard_stable_storage *stable_storage) {
    return stable_storage->api.tx_commit(stable_storage->handle);
}

void
storage_tx_abort(struct standard_stable_storage *stable_storage) {
    stable_storage->api.tx_abort(stable_storage->handle);
}

int
storage_store_trim_instance(struct standard_stable_storage *stable_storage, const iid_t iid) {
    return stable_storage->api.store_trim_instance(stable_storage->handle, iid);
}

// TODO FIX UP ALL THESE POINTER THINGS
// WORK OUT SHOULD WE BE EXPECTING VOID * OR CAN IT BE DONE IN HERE
int
storage_get_trim_instance(struct standard_stable_storage *stable_storage, iid_t *instance_id) {
    return stable_storage->api.get_trim_instance(stable_storage->handle, instance_id);
    //get_trim_instance(standard_stable_storage, &instance_id);
}


// returning 0 means not found
// returning 1 means found
int
storage_get_instance_info(struct standard_stable_storage *store, iid_t instance_id, struct paxos_accepted *instance_info_retrieved) {
    return store->api.get_instance_info(store->handle, instance_id, instance_info_retrieved);
}

int storage_store_instance_info(struct standard_stable_storage *store, const struct paxos_accepted *instance_info) {
    return store->api.store_instance_info(store->handle, instance_info);
}

int storage_get_all_untrimmed_instances_info(struct standard_stable_storage *store, paxos_accepted **retrieved_instances_info,
                                             int *number_of_instances_retrieved) {
    return store->api.get_all_untrimmed_instances_info(store->handle, retrieved_instances_info,
                                                       number_of_instances_retrieved);
}

int storage_get_max_inited_instance(struct standard_stable_storage *storage, iid_t *retrieved_instance) {
    return storage->api.get_max_instance(storage->handle, retrieved_instance);
}
/*
int storage_get_last_promise(struct standard_stable_storage* store, iid_t instance_id, paxos_prepare* last_promise_retrieved){
    return get_last_promise(store->paxos_storage, instance_id, last_promise_retrieved);
}

int storage_store_last_promise(struct standard_stable_storage* store, paxos_prepare* last_promise){
    return store_last_prepare(store->paxos_storage, last_promise);
}

int storage_get_last_promises(struct standard_stable_storage* store, iid_t* instance_ids, int number_of_instances_to_retrieve, paxos_prepare** retrieved_last_promises, int* number_of_instances_retrieved){
    return get_last_promises(store->paxos_storage, instance_ids, number_of_instances_to_retrieve, retrieved_last_promises, number_of_instances_retrieved);
}
int storage_store_last_promises(struct standard_stable_storage* store, paxos_prepare** last_promises, int number_of_promises){
    return store_last_promises(store->paxos_storage, last_promises, number_of_promises);
}

int storage_get_last_accepted(struct standard_stable_storage* store, iid_t instance_id, paxos_accept* last_accepted_retrieved){
    return get_last_accept(store->paxos_storage, instance_id, last_accepted_retrieved);
}

int storage_store_last_accepted(struct standard_stable_storage* store, paxos_accept* last_accepted){
    return store_acceptance(store->paxos_storage, last_accepted);
}

int storage_get_last_accepteds(struct standard_stable_storage* store, iid_t* instance_ids, int number_of_instances_to_retrieve, paxos_accept** last_accepteds_retrieved){
    return get_last_accepteds(store->paxos_storage, instance_ids, number_of_instances_to_retrieve, last_accepteds_retrieved);
}

int storage_store_last_accepteds(struct standard_stable_storage* store, paxos_accept** ACCEPT_MAP_SYMBOL_AND_NAME, int number_of_instances){
    return store_acceptances(store->paxos_storage, ACCEPT_MAP_SYMBOL_AND_NAME, number_of_instances);
}

int storage_get_instance_info(struct standard_stable_storage* store, iid_t instance_id, paxos_accepted* instance_info_retrieved){
    return  get_instance_info(store->paxos_storage, instance_id, instance_info_retrieved);
}

int storage_store_instance_info(struct standard_stable_storage* store, paxos_accepted* instance_info){
    return store_instance_info(store->paxos_storage, instance_info);
}

int storage_get_all_untrimmed_instances_info(struct standard_stable_storage* store, paxos_accepted** retrieved_instances_info, int number_of_instances_retrieved){
    return get_all_untrimmed_instances_info(store->paxos_storage, retrieved_instances_info, number_of_instances_retrieved);
}

*/

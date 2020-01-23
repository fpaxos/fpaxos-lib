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


#ifndef _STORAGE_H_
#define _STORAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "paxos.h"
#include "paxos_storage.h"

struct stable_storage {
    void *handle;

    //struct paxos_storage* paxos_storage;
    struct {
        int (*open)(void *handle);

        void (*close)(void *handle);

        int (*tx_begin)(void *handle);

        int (*tx_commit)(void *handle);

        void (*tx_abort)(void *handle);

        int (*store_trim_instance)(void *handle, const iid_t trim_instance_id);

        int (*get_trim_instance)(void *handle, iid_t *trim_instance_id_retrieved);

        int (*store_instance_info)(void *handle, const struct paxos_accepted *instance_info_to_store);

        int (*get_instance_info)(void *handle, const iid_t instance_id, paxos_accepted *instance_info_retrieved);

        int (*get_all_untrimmed_instances_info)(void *handle, paxos_accepted **retrieved_instances_info,
                                                int *number_of_instances_retrieved);

        int (*get_max_instance)(void *handle, iid_t *retrieved_instance);
    } stable_storage_api;
};

void storage_init(struct stable_storage *store, int acceptor_id);

int storage_open(struct stable_storage *stable_storage);

void storage_close(struct stable_storage *store);

int storage_tx_begin(struct stable_storage *store);

int storage_tx_commit(struct stable_storage *store);

void storage_tx_abort(struct stable_storage *store);

int storage_store_trim_instance(struct stable_storage *stable_storage, const iid_t iid);

int storage_get_trim_instance(struct stable_storage *store, iid_t *instance_id);

/*
int storage_get_last_promise(struct stable_storage* store, iid_t instance_id, paxos_prepare* last_promise_retrieved);
int storage_store_last_promise(struct stable_storage* store, paxos_prepare* last_promise);
int storage_get_last_promises(struct stable_storage* store, iid_t* instance_ids, int number_of_instances_to_retrieve, paxos_prepare** retrieved_last_promises, int* number_of_instances_retrieved);
int storage_store_last_promises(struct stable_storage* store, paxos_prepare** last_promises, int number_of_promises);

int storage_get_last_accepted(struct stable_storage* store, iid_t instance_id, paxos_accept* last_accepted_retrieved);
int storage_store_last_accepted(struct stable_storage* store, paxos_accept* last_accepted);
int storage_get_last_accepteds(struct stable_storage* store, iid_t* instance_ids, int number_of_instances_to_retrieve, paxos_accept** last_accepteds_retrieved);
int storage_store_last_accepteds(struct stable_storage* store, paxos_accept** last_accepteds, int number_of_instances);
*/
int storage_get_instance_info(struct stable_storage *store, const iid_t instance_id, paxos_accepted *instance_info_retrieved);

int storage_store_instance_info(struct stable_storage *store, const struct paxos_accepted *instance_info);

int storage_get_all_untrimmed_instances_info(struct stable_storage *store, struct paxos_accepted **retrieved_instances_info,
                                             int *number_of_instances_retrieved);

int storage_get_max_inited_instance(struct stable_storage *storage, iid_t *retrieved_instance);

void storage_init_mem(struct stable_storage *s, int acceptor_id);

void storage_init_lmdb(struct stable_storage *s, int acceptor_id);

#ifdef __cplusplus
}
#endif

#endif

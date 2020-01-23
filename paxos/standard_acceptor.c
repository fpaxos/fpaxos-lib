/*
 * Copyright (c) 2013-2014, University of Lugano
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


#include "standard_acceptor.h"
#include "stable_storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <paxos_message_conversion.h>

/*
struct standard_acceptor
{
	int id;
	iid_t trim_iid;
    struct stable_storage stable_storage;
};
*/


struct standard_acceptor*
standard_acceptor_new(int id)
{
	struct standard_acceptor* a = calloc(1, sizeof(struct standard_acceptor));

    storage_init(&a->stable_storage, id);
    if (storage_open(&a->stable_storage) != 0) {
		free(a);
		return NULL;
	}
    if (storage_tx_begin(&a->stable_storage) != 0)
		return NULL;
	a->id = id;

	iid_t * trim_iid = calloc(1, sizeof(iid_t));
    storage_get_trim_instance(&a->stable_storage, trim_iid);
    a->trim_iid = *trim_iid;

    if (storage_tx_commit(&a->stable_storage) != 0)
		return NULL;
	return a;
}

void
standard_acceptor_free(struct standard_acceptor *a) {
    storage_close(&a->stable_storage);
	free(a);
}

int
standard_acceptor_receive_prepare(struct standard_acceptor *a,
                                  paxos_prepare *req, paxos_message *out)
{
	paxos_accepted acc;
	if (req->iid <= a->trim_iid)
		return 0;
	memset(&acc, 0, sizeof(paxos_accepted));
    if (storage_tx_begin(&a->stable_storage) != 0)
		return 0;
    int found = storage_get_instance_info(&a->stable_storage, req->iid, &acc);
	if (!found || acc.ballot <= req->ballot) {
		paxos_log_debug("Preparing iid: %u, ballot: %u", req->iid, req->ballot);
		acc.aid = a->id;
		acc.iid = req->iid;
		acc.ballot = req->ballot;
        if (storage_store_instance_info(&a->stable_storage, &acc) != 0) {
            storage_tx_abort(&a->stable_storage);
			return 0;
		}
	}
    if (storage_tx_commit(&a->stable_storage) != 0)
		return 0;
    paxos_accepted_to_promise(&acc, out);
	return 1;
}

int
standard_acceptor_receive_accept(struct standard_acceptor *a,
                                 paxos_accept *req, paxos_message *out)
{
	paxos_accepted acc;
	if (req->iid <= a->trim_iid)
		return 0;
	memset(&acc, 0, sizeof(paxos_accepted));
    if (storage_tx_begin(&a->stable_storage) != 0)
		return 0;
    int found = storage_get_instance_info(&a->stable_storage, req->iid, &acc);
	if (!found || acc.ballot <= req->ballot) {
		paxos_log_debug("Accepting iid: %u, ballot: %u", req->iid, req->ballot);
		paxos_accept_to_accepted(a->id, req, out);
        if (storage_store_instance_info(&a->stable_storage, &(out->u.accepted)) != 0) {
            storage_tx_abort(&a->stable_storage);
			return 0;
		}
	} else {
		paxos_accepted_to_preempted(a->id, &acc, out);
	}
    if (storage_tx_commit(&a->stable_storage) != 0)
		return 0;
	paxos_accepted_destroy(&acc);
	return 1;
}

int
standard_acceptor_receive_repeat(struct standard_acceptor *a, iid_t iid, paxos_accepted *out)
{
	memset(out, 0, sizeof(paxos_accepted));
    if (storage_tx_begin(&a->stable_storage) != 0)
		return 0;
    int found = storage_get_instance_info(&a->stable_storage, iid, out);
    if (storage_tx_commit(&a->stable_storage) != 0)
		return 0;
	return found && (out->value.paxos_value_len > 0);
}

int
standard_acceptor_receive_trim(struct standard_acceptor *a, paxos_trim *trim)
{
	if (trim->iid <= a->trim_iid)
		return 0;
	a->trim_iid = trim->iid;
    if (storage_tx_begin(&a->stable_storage) != 0)
		return 0;
    storage_store_trim_instance(&a->stable_storage, trim->iid);
    if (storage_tx_commit(&a->stable_storage) != 0)
		return 0;
	return 1;
}

void
standard_acceptor_set_current_state(struct standard_acceptor *a, paxos_standard_acceptor_state *state)
{
	state->aid = a->id;
	state->trim_iid = a->trim_iid;
}

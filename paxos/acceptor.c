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


#include "acceptor.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>

struct acceptor
{
	int id;
	iid_t trim_iid;
	struct storage store;
};

static void paxos_accepted_to_promise(paxos_accepted* acc, paxos_message* out);
static void paxos_accept_to_accepted(int id, paxos_accept* acc, paxos_message* out);
static void paxos_accepted_to_preempted(int id, paxos_accepted* acc, paxos_message* out);


struct acceptor*
acceptor_new(int id)
{
	struct acceptor* a;
	a = malloc(sizeof(struct acceptor));
	storage_init(&a->store, id);
	if (storage_open(&a->store) != 0) {
		free(a);
		return NULL;
	}
	if (storage_tx_begin(&a->store) != 0)
		return NULL;
	a->id = id;
	a->trim_iid = storage_get_trim_instance(&a->store);
	if (storage_tx_commit(&a->store) != 0)
		return NULL;
	return a;
}

void
acceptor_free(struct acceptor* a) 
{
	storage_close(&a->store);
	free(a);
}

int
acceptor_receive_prepare(struct acceptor* a, 
	paxos_prepare* req, paxos_message* out)
{
	paxos_accepted acc;
	if (req->iid <= a->trim_iid)
		return 0;
	memset(&acc, 0, sizeof(paxos_accepted));
	if (storage_tx_begin(&a->store) != 0)
		return 0;
	int found = storage_get_record(&a->store, req->iid, &acc);
	if (!found || acc.ballot <= req->ballot) {
		paxos_log_debug("Preparing iid: %u, ballot: %u", req->iid, req->ballot);
		acc.aid = a->id;
		acc.iid = req->iid;
		acc.ballot = req->ballot;
		if (storage_put_record(&a->store, &acc) != 0) {
			storage_tx_abort(&a->store);
			return 0;
		}
	}
	if (storage_tx_commit(&a->store) != 0)
		return 0;
	paxos_accepted_to_promise(&acc, out);
	return 1;
}

int
acceptor_receive_accept(struct acceptor* a,
	paxos_accept* req, paxos_message* out)
{
	paxos_accepted acc;
	if (req->iid <= a->trim_iid)
		return 0;
	memset(&acc, 0, sizeof(paxos_accepted));
	if (storage_tx_begin(&a->store) != 0)
		return 0;
	int found = storage_get_record(&a->store, req->iid, &acc);
	if (!found || acc.ballot <= req->ballot) {
		paxos_log_debug("Accepting iid: %u, ballot: %u", req->iid, req->ballot);
		paxos_accept_to_accepted(a->id, req, out);
		if (storage_put_record(&a->store, &(out->u.accepted)) != 0) {
			storage_tx_abort(&a->store);
			return 0;
		}
	} else {
		paxos_accepted_to_preempted(a->id, &acc, out);
	}
	if (storage_tx_commit(&a->store) != 0)
		return 0;
	paxos_accepted_destroy(&acc);
	return 1;
}

int
acceptor_receive_repeat(struct acceptor* a, iid_t iid, paxos_accepted* out)
{
	memset(out, 0, sizeof(paxos_accepted));
	if (storage_tx_begin(&a->store) != 0)
		return 0;
	int found = storage_get_record(&a->store, iid, out);
	if (storage_tx_commit(&a->store) != 0)
		return 0;
	return found && (out->value.paxos_value_len > 0);
}

int
acceptor_receive_trim(struct acceptor* a, paxos_trim* trim)
{
	if (trim->iid <= a->trim_iid)
		return 0;
	a->trim_iid = trim->iid;
	if (storage_tx_begin(&a->store) != 0)
		return 0;
	storage_trim(&a->store, trim->iid);
	if (storage_tx_commit(&a->store) != 0)
		return 0;
	return 1;
}

void
acceptor_set_current_state(struct acceptor* a, paxos_acceptor_state* state)
{
	state->aid = a->id;
	state->trim_iid = a->trim_iid;
}

static void
paxos_accepted_to_promise(paxos_accepted* acc, paxos_message* out)
{
	out->type = PAXOS_PROMISE;
	out->u.promise = (paxos_promise) {
		acc->aid,
		acc->iid,
		acc->ballot,
		acc->value_ballot,
		{acc->value.paxos_value_len, acc->value.paxos_value_val}
	};
}

static void
paxos_accept_to_accepted(int id, paxos_accept* acc, paxos_message* out)
{
	char* value = NULL;
	int value_size = acc->value.paxos_value_len;
	if (value_size > 0) {
		value = malloc(value_size);
		memcpy(value, acc->value.paxos_value_val, value_size);
	}
	out->type = PAXOS_ACCEPTED;
	out->u.accepted = (paxos_accepted) {
		id,
		acc->iid,
		acc->ballot,
		acc->ballot,
		{value_size, value}
	};
}

static void
paxos_accepted_to_preempted(int id, paxos_accepted* acc, paxos_message* out)
{
	out->type = PAXOS_PREEMPTED;
	out->u.preempted = (paxos_preempted) { id, acc->iid, acc->ballot };
}

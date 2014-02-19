/*
	Copyright (c) 2013, University of Lugano
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
    	* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the copyright holders nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
*/


#include "acceptor.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>

struct acceptor
{
	struct storage store;
};

static void paxos_accepted_to_promise(paxos_accepted* acc, paxos_promise* out);
static void paxos_accept_to_accepted(paxos_accept* acc, paxos_accepted* out);


struct acceptor*
acceptor_new(int id)
{
	struct acceptor* a;
	a = malloc(sizeof(struct acceptor));
	storage_init(&a->store, id);
	if (storage_open(&a->store) < 0) {
		free(a);
		return NULL;
	}
	return a;
}

int
acceptor_free(struct acceptor* a) 
{
	int rv;
	rv = storage_close(&a->store);
	free(a);
	return rv;
}

int
acceptor_receive_prepare(struct acceptor* a, 
	paxos_prepare* req, paxos_promise* out)
{
	paxos_accepted acc;
	memset(&acc, 0, sizeof(paxos_accepted));
	storage_tx_begin(&a->store);
	int found = storage_get_record(&a->store, req->iid, &acc);
	if (!found || acc.ballot <= req->ballot) {
		paxos_log_debug("Preparing iid: %u, ballot: %u", req->iid, req->ballot);
		acc.iid = req->iid;
		acc.ballot = req->ballot;
		storage_put_record(&a->store, &acc);
	}
	storage_tx_commit(&a->store);
	paxos_accepted_to_promise(&acc, out);
	return 1;
}

int
acceptor_receive_accept(struct acceptor* a,
	paxos_accept* req, paxos_accepted* out)
{
	int accepted = 0;
	memset(out, 0, sizeof(paxos_accepted));
	storage_tx_begin(&a->store);
	int found = storage_get_record(&a->store, req->iid, out);
	if (!found || out->ballot <= req->ballot) {
		paxos_log_debug("Accepting iid: %u, ballot: %u", req->iid, req->ballot);
		paxos_accepted_destroy(out);
		paxos_accept_to_accepted(req, out);
		storage_put_record(&a->store, out);
		accepted = 1;
	}
	storage_tx_commit(&a->store);
	return accepted;
}

int
acceptor_receive_repeat(struct acceptor* a, iid_t iid, paxos_accepted* out)
{
	memset(out, 0, sizeof(paxos_accepted));
	storage_tx_begin(&a->store);
	int found = storage_get_record(&a->store, iid, out);
	storage_tx_commit(&a->store);
	return found;
}

static void
paxos_accepted_to_promise(paxos_accepted* acc, paxos_promise* out)
{
	*out = (paxos_promise) {
		acc->iid,
		acc->ballot,
		acc->value_ballot,
		{acc->value.paxos_value_len, acc->value.paxos_value_val}
	};
}

static void
paxos_accept_to_accepted(paxos_accept* acc, paxos_accepted* out)
{
	char* value = NULL;
	int value_size = acc->value.paxos_value_len;
	if (value_size > 0) {
		value = malloc(value_size);
		memcpy(value, acc->value.paxos_value_val, value_size);
	}
	out->iid = acc->iid;
	out->ballot = acc->ballot;
	out->value_ballot = acc->ballot;
	out->value.paxos_value_len = value_size;
	out->value.paxos_value_val = value;
}

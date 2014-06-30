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

struct acceptor
{
	struct storage* store;
};

static acceptor_record*
apply_prepare(struct storage* s, prepare_req* ar, acceptor_record* rec);

static acceptor_record*
apply_accept(struct storage* s, accept_req* ar, acceptor_record* rec);


struct acceptor*
acceptor_new(int id)
{
	struct acceptor* s;
	s = malloc(sizeof(struct acceptor));
	s->store = storage_open(id); 
	if (s->store == NULL) {
		free(s);
		return NULL;	
	}
	return s;
}

int
acceptor_free(struct acceptor* a) 
{
	int rv;
	rv = storage_close(a->store);
	free(a);
	return rv;
}

void
acceptor_free_record(struct acceptor* a, acceptor_record* r)
{
	storage_free_record(a->store, r);
}

acceptor_record*
acceptor_receive_prepare(struct acceptor* a, prepare_req* req)
{
	acceptor_record* rec;
	storage_tx_begin(a->store);
	rec = storage_get_record(a->store, req->iid);
	rec = apply_prepare(a->store, req, rec);
	storage_tx_commit(a->store);
	return rec;
}

acceptor_record*
acceptor_receive_accept(struct acceptor* a, accept_req* req)
{
	acceptor_record* rec;
	storage_tx_begin(a->store);
	rec = storage_get_record(a->store, req->iid);
	rec = apply_accept(a->store, req, rec);
	storage_tx_commit(a->store);
	return rec;
}

acceptor_record*
acceptor_receive_repeat(struct acceptor* a, iid_t iid)
{
	acceptor_record* rec;
	storage_tx_begin(a->store);
	rec = storage_get_record(a->store, iid);
	storage_tx_commit(a->store);
	return rec;
}

static acceptor_record*
apply_prepare(struct storage* s, prepare_req* pr, acceptor_record* rec)
{
	// We already have a more recent ballot
	if (rec != NULL && rec->ballot >= pr->ballot) {
		paxos_log_debug("Prepare iid: %u dropped (ballots curr:%u recv:%u)",
			pr->iid, rec->ballot, pr->ballot);
		return rec;
	}
	
	// Stored value is final, the instance is closed already
	if (rec != NULL && rec->is_final) {
		paxos_log_debug("Prepare request for iid: %u dropped \
			(stored value is final)", pr->iid);
		return rec;
	}
	
	// Record not found or smaller ballot, in both cases overwrite and store
	paxos_log_debug("Preparing iid: %u, ballot: %u", pr->iid, pr->ballot);
	
	if (rec != NULL)	{
		storage_free_record(s, rec);
	}

	// Store the updated record
	return storage_save_prepare(s, pr);
}

static acceptor_record*
apply_accept(struct storage* s, accept_req* ar, acceptor_record* rec)
{
	// We already have a more recent ballot
	if (rec != NULL && rec->ballot > ar->ballot) {
		paxos_log_debug("Accept for iid:%u dropped (ballots curr:%u recv:%u)",
			ar->iid, rec->ballot, ar->ballot);
		return rec;
	}
	
	// Record not found or smaller ballot, in both cases overwrite and store
	paxos_log_debug("Accepting iid: %u, ballot: %u", ar->iid, ar->ballot);
	
	if (rec != NULL) {
		storage_free_record(s, rec);
	}

	// Store the updated record
	return storage_save_accept(s, ar);
}

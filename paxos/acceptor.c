/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "acceptor.h"
#include "storage.h"
#include <stdlib.h>

struct acceptor
{
	struct storage* store;
};

static acceptor_record*
apply_prepare(struct storage* s, paxos_prepare* ar, acceptor_record* rec);

static acceptor_record*
apply_accept(struct storage* s, paxos_accept* ar, acceptor_record* rec);


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

int
acceptor_receive_prepare(struct acceptor* a, 
	paxos_prepare* req, paxos_promise* out)
{
	acceptor_record* rec;
	storage_tx_begin(a->store);
	rec = storage_get_record(a->store, req->iid);
	rec = apply_prepare(a->store, req, rec);
	storage_tx_commit(a->store);
	
	*out = (paxos_promise) {
		rec->iid,
		rec->ballot,
		rec->value_ballot,
		{ rec->value.value_len, rec->value.value_val }
	};

	return 1;
}

int
acceptor_receive_accept(struct acceptor* a,
	paxos_accept* req, paxos_accepted* out)
{
	acceptor_record* rec;
	storage_tx_begin(a->store);
	rec = storage_get_record(a->store, req->iid);
	rec = apply_accept(a->store, req, rec);
	storage_tx_commit(a->store);
	*out = *rec;
	return 1;
}

int
acceptor_receive_repeat(struct acceptor* a, iid_t iid, paxos_accepted* out)
{
	acceptor_record* rec;
	storage_tx_begin(a->store);
	rec = storage_get_record(a->store, iid);
	storage_tx_commit(a->store);
	if (rec == NULL)
		return 0;
	*out = *rec;
	return 1;
}

static acceptor_record*
apply_prepare(struct storage* s, paxos_prepare* pr, acceptor_record* rec)
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
	
	// Store the updated record
	return storage_save_prepare(s, pr, rec);
}

static acceptor_record*
apply_accept(struct storage* s, paxos_accept* ar, acceptor_record* rec)
{
	// We already have a more recent ballot
	if (rec != NULL && rec->ballot > ar->ballot) {
		paxos_log_debug("Accept for iid:%u dropped (ballots curr:%u recv:%u)",
			ar->iid, rec->ballot, ar->ballot);
		return rec;
	}
	
	// Record not found or smaller ballot, in both cases overwrite and store
	paxos_log_debug("Accepting iid: %u, ballot: %u", ar->iid, ar->ballot);
	
	// Store the updated record
	return storage_save_accept(s, ar);
}

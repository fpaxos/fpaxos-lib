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

struct acceptor {
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
acceptor_free(struct acceptor* s) 
{
	int rv;
	rv = storage_close(s->store);
	free(s);
	return rv;
}

acceptor_record*
acceptor_receive_prepare(struct acceptor* s, prepare_req* req)
{
	acceptor_record* rec;
	storage_tx_begin(s->store);
	rec = storage_get_record(s->store, req->iid);
	rec = apply_prepare(s->store, req, rec);
	storage_tx_commit(s->store);
	return rec;
}

acceptor_record*
acceptor_receive_accept(struct acceptor* s, accept_req* req)
{
	acceptor_record* rec;
	storage_tx_begin(s->store);
	rec = storage_get_record(s->store, req->iid);
	rec = apply_accept(s->store, req, rec);
	storage_tx_commit(s->store);
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
		LOG(DBG, ("Prepare iid:%u dropped (ballots curr:%u recv:%u)\n", 
			pr->iid, rec->ballot, pr->ballot));
		return rec;
	}
	
	// Stored value is final, the instance is closed already
	if (rec != NULL && rec->is_final) {
		LOG(DBG, ("Prepare request for iid:%u dropped \
			(stored value is final)\n", pr->iid));
		return rec;
	}
	
	// Record not found or smaller ballot
	// in both cases overwrite and store
	LOG(DBG, ("Prepare request is valid for iid:%u (ballot:%u)\n", 
		pr->iid, pr->ballot));
	
	// Store the updated record
	return storage_save_prepare(s, pr, rec);
}

static acceptor_record*
apply_accept(struct storage* s, accept_req* ar, acceptor_record* rec)
{
	// We already have a more recent ballot
	if (rec != NULL && rec->ballot > ar->ballot) {
		LOG(DBG, ("Accept for iid:%u dropped (ballots curr:%u recv:%u)\n", 
			ar->iid, rec->ballot, ar->ballot));
		return rec;
	}
	
	// Record not found or smaller ballot
	// in both cases overwrite and store
	LOG(DBG, ("Accepting for iid:%u (ballot:%u)\n", 
		ar->iid, ar->ballot));
	
	// Store the updated record
	return storage_save_accept(s, ar);
}

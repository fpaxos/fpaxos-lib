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
	/*
		TODO Acceptors should be able to "recover" from the existing BDB file. 
		For now we just force do_recovery to be false (BDB creates a new
		database from scratch each time the acceptor starts).
	*/
	int do_recovery;
	if (DURABILITY_MODE > 0) do_recovery = 1;
	struct acceptor* s;
	s = malloc(sizeof(struct acceptor));
	s->store = storage_open(id, do_recovery); 
	if (s->store == NULL) {
		free(s);
		return NULL;	
	}
	return s;
}

int
acceptor_delete(struct acceptor* s) 
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

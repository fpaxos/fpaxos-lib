#include "acceptor_state.h"
#include "storage.h"
#include <stdlib.h>

// TODO this should be configurable
#define BDB_MODE 0

struct acceptor_state {
	struct storage* store;
};

static acceptor_record*
apply_prepare(struct storage* s, prepare_req* ar, acceptor_record* rec);
	
static acceptor_record*
apply_accept(struct storage* s, accept_req* ar, acceptor_record* rec);


struct acceptor_state*
acceptor_state_new(int id)
{
	struct acceptor_state* s;
	s = malloc(sizeof(struct acceptor_state));
	s->store = storage_open(id, BDB_MODE);
	if (s->store == NULL) {
		free(s);
		return NULL;	
	}
	return s;
}

int
acceptor_state_delete(struct acceptor_state* s) 
{
	int rv;
	rv = storage_close(s->store);
	free(s);
	return rv;
}

acceptor_record*
acceptor_state_receive_prepare(struct acceptor_state* s, prepare_req* req)
{
	acceptor_record* rec;
    storage_tx_begin(s->store);
	rec = storage_get_record(s->store, req->iid);
	rec = apply_prepare(s->store, req, rec);
    storage_tx_commit(s->store);
	return rec;
}

acceptor_record*
acceptor_state_receive_accept(struct acceptor_state* s, accept_req* req)
{
	acceptor_record* rec;
	storage_tx_begin(s->store);
	rec = storage_get_record(s->store, req->iid);
	rec = apply_accept(s->store, req, rec);
	storage_tx_commit(s->store);
	return rec;
}

// Given a prepare (phase 1a) request message and the
// corresponding record, will update if the request is valid.
// Returns the new record if the promise was made, otherwise NULL.
static acceptor_record*
apply_prepare(struct storage* s, prepare_req* pr, acceptor_record* rec)
{
    //We already have a more recent ballot
    if (rec != NULL && rec->ballot >= pr->ballot) {
        LOG(DBG, ("Prepare iid:%u dropped (ballots curr:%u recv:%u)\n", 
            pr->iid, rec->ballot, pr->ballot));
        return NULL;
    }
    
    //Stored value is final, the instance is closed already
    if (rec != NULL && rec->is_final) {
        LOG(DBG, ("Prepare request for iid:%u dropped \
            (stored value is final)\n", pr->iid));
        return NULL;
    }
    
    //Record not found or smaller ballot
    // in both cases overwrite and store
    LOG(DBG, ("Prepare request is valid for iid:%u (ballot:%u)\n", 
        pr->iid, pr->ballot));
    
    //Store the updated record
    rec = storage_save_prepare(s, pr, rec);

    return rec;
}

// Given an accept request (phase 2a) message and the current record
// will update the record if the request is legal.
// Returns the new record if the accept was applied, NULL otherwise.
static acceptor_record*
apply_accept(struct storage* s, accept_req* ar, acceptor_record* rec)
{
    // We already have a more recent ballot
    if (rec != NULL && rec->ballot < ar->ballot) {
        LOG(DBG, ("Accept for iid:%u dropped (ballots curr:%u recv:%u)\n", 
            ar->iid, rec->ballot, ar->ballot));
        return NULL;
    }
    
    // Record not found or smaller ballot
    // in both cases overwrite and store
    LOG(DBG, ("Accepting for iid:%u (ballot:%u)\n", 
        ar->iid, ar->ballot));
    
    // Store the updated record
    rec = storage_save_accept(s, ar);
    
    return rec;
}

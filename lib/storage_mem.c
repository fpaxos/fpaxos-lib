#include "storage.h"
#include "paxos_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef ACCEPTOR_STORAGE_MEM

#define MAX_SIZE_RECORD (8*1024)
#define MAX_RECORDS (4*1024)

struct storage
{
	int acceptor_id;
	acceptor_record *records[MAX_RECORDS];
};

struct storage*
storage_open(int acceptor_id, int do_recovery)
{
	struct storage* s = malloc(sizeof(struct storage));
	assert(s != NULL);
	memset(s, 0, sizeof(struct storage));
	
	int i;
	for (i=0; i<MAX_RECORDS; ++i){
		s->records[i] = (acceptor_record *) malloc(MAX_SIZE_RECORD);
		assert(s->records[i] != NULL);
		s->records[i]->iid = 0;
	}

	s->acceptor_id = acceptor_id;

	return s;
}

int
storage_close(struct storage* s)
{
	int i;
	for (i=0; i<MAX_RECORDS; ++i){
		free(s->records[i]);
	}
	
	free(s);
	
	return 0;
}

void
storage_tx_begin(struct storage* s)
{
	return;
}

void
storage_tx_commit(struct storage* s)
{
	return;
}

acceptor_record* 
storage_get_record(struct storage* s, iid_t iid)
{
	acceptor_record* record_buffer = s->records[iid % MAX_RECORDS];
	if (iid < record_buffer->iid){
		fprintf(stderr, "instance too old %d: current is %d", iid, record_buffer->iid);
		exit(1);
	}
	if (iid == record_buffer->iid){
		return record_buffer;
	} else {
		return NULL;
	}
}

acceptor_record*
storage_save_accept(struct storage* s, accept_req * ar)
{
	acceptor_record* record_buffer = s->records[ar->iid % MAX_RECORDS];
	
	//Store as acceptor_record (== accept_ack)
	record_buffer->acceptor_id = s->acceptor_id;
	record_buffer->iid = ar->iid;
	record_buffer->ballot = ar->ballot;
	record_buffer->value_ballot = ar->ballot;
	record_buffer->is_final = 0;
	record_buffer->value_size = ar->value_size;
	memcpy(record_buffer->value, ar->value, ar->value_size);
	
	return record_buffer;
}

acceptor_record*
storage_save_prepare(struct storage* s, prepare_req* pr, acceptor_record* rec)
{
	acceptor_record* record_buffer = s->records[pr->iid % MAX_RECORDS];
	
	//No previous record, create a new one
	if (rec == NULL) {
		//Record does not exist yet
		rec = record_buffer;
		rec->acceptor_id = s->acceptor_id;
		rec->iid = pr->iid;
		rec->ballot = pr->ballot;
		rec->value_ballot = 0;
		rec->is_final = 0;
		rec->value_size = 0;
	} else {
		//Record exists, just update the ballot
		rec->ballot = pr->ballot;
	}
	
	return record_buffer;
}

acceptor_record*
storage_save_final_value(struct storage* s, char* value, size_t size, 
	iid_t iid, ballot_t b)
{
	acceptor_record* record_buffer = s->records[iid % MAX_RECORDS];
	
	//Store as acceptor_record (== accept_ack)
	record_buffer->iid = iid;
	record_buffer->ballot = b;
	record_buffer->value_ballot = b;
	record_buffer->is_final = 1;
	record_buffer->value_size = size;
	memcpy(record_buffer->value, value, size);
	
		return record_buffer;
}

#endif

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


#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_RECORDS (4*1024)

struct storage
{
	int acceptor_id;
	acceptor_record *records[MAX_RECORDS];
};


struct storage*
storage_open(int acceptor_id)
{
	int i;
	struct storage* s = malloc(sizeof(struct storage));
	memset(s, 0, sizeof(struct storage));
	for (i = 0; i < MAX_RECORDS; ++i) {
		s->records[i] = malloc(sizeof(acceptor_record));
		memset(s->records[i], 0, sizeof(acceptor_record));
	}
	s->acceptor_id = acceptor_id;
	return s;
}

int
storage_close(struct storage* s)
{
	int i;
	for (i = 0; i < MAX_RECORDS; ++i) {
		if (s->records[i]->value.value_val != NULL)
			free(s->records[i]->value.value_val);
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
	if (iid < record_buffer->iid) {
		paxos_log_error("Instance %d too old! Current is %d",
			iid, record_buffer->iid);
		exit(1);
	}
	if (iid == record_buffer->iid)
		return record_buffer;
	return NULL;
}

acceptor_record*
storage_save_accept(struct storage* s, paxos_accept* ar)
{
	acceptor_record* record_buffer = s->records[ar->iid % MAX_RECORDS];
	
	//Store as acceptor_record (== accept_ack)
	record_buffer->acceptor_id = s->acceptor_id;
	record_buffer->iid = ar->iid;
	record_buffer->ballot = ar->ballot;
	record_buffer->value_ballot = ar->ballot;
	record_buffer->is_final = 0;
	record_buffer->value.value_len = 0;
	record_buffer->value.value_val = NULL;
	if (ar->value.value_val != NULL) {
		record_buffer->value.value_len = ar->value.value_len;
		record_buffer->value.value_val = malloc(ar->value.value_len);
		memcpy(record_buffer->value.value_val,
			ar->value.value_val, ar->value.value_len);
	}
	return record_buffer;
}

acceptor_record*
storage_save_prepare(struct storage* s, paxos_prepare* pr, acceptor_record* rec)
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
		rec->value.value_len = 0;
		rec->value.value_val = NULL;
	} else {
		//Record exists, just update the ballot
		rec->ballot = pr->ballot;
		// if (rec->value.value_val != NULL) {
		// 	free()
		// }
	}
	
	return record_buffer;
}

acceptor_record*
storage_save_final_value(struct storage* s, char* value, size_t size, 
	iid_t iid, ballot_t b)
{
	acceptor_record* record_buffer = s->records[iid % MAX_RECORDS];
	
	record_buffer->iid = iid;
	record_buffer->ballot = b;
	record_buffer->value_ballot = b;
	record_buffer->is_final = 1;
	// record_buffer->value = paxos_value_new(value, size);
	
	return record_buffer;
}

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
	struct storage* s = malloc(sizeof(struct storage));
	assert(s != NULL);
	memset(s, 0, sizeof(struct storage));
	
	s->acceptor_id = acceptor_id;

	return s;
}

int
storage_close(struct storage* s)
{
	int i;
	for (i=0; i<MAX_RECORDS; ++i) {
		if (s->records[i] != NULL) {
			free(s->records[i]);
		}
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

void
storage_free_record(struct storage* s, acceptor_record* r) 
{
	return;
}

acceptor_record* 
storage_get_record(struct storage* s, iid_t iid)
{
	acceptor_record* record_buffer = s->records[iid % MAX_RECORDS];
	if (record_buffer == NULL) {
		return NULL;
	}

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
storage_save_accept(struct storage* s, accept_req* ar)
{
	acceptor_record* record_buffer = s->records[ar->iid % MAX_RECORDS];
	if (record_buffer == NULL) {
		record_buffer = malloc(ACCEPT_RECORD_BUFF_SIZE(ar->value_size));
		assert(record_buffer != NULL);
		s->records[ar->iid % MAX_RECORDS] = record_buffer;
	}
	
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
storage_save_prepare(struct storage* s, prepare_req* pr)
{
	acceptor_record* record_buffer = s->records[pr->iid % MAX_RECORDS];
	if (record_buffer == NULL) {
		record_buffer = malloc(ACCEPT_RECORD_BUFF_SIZE(0));
		assert(record_buffer != NULL);
		s->records[pr->iid % MAX_RECORDS] = record_buffer;

		// Setup new record
		record_buffer->acceptor_id = s->acceptor_id;
		record_buffer->iid = pr->iid;
		record_buffer->value_ballot = 0;
		record_buffer->is_final = 0;
		record_buffer->value_size = 0;
	}
	// Always set or update the ballot.
	record_buffer->ballot = pr->ballot;

	return record_buffer;
}

acceptor_record*
storage_save_final_value(struct storage* s, char* value, size_t size, 
	iid_t iid, ballot_t b)
{
	acceptor_record* record_buffer = s->records[iid % MAX_RECORDS];
	if (record_buffer == NULL) {
		record_buffer = malloc(ACCEPT_RECORD_BUFF_SIZE(size));
		assert(record_buffer != NULL);
		s->records[iid % MAX_RECORDS] = record_buffer;
	}

	record_buffer->iid = iid;
	record_buffer->ballot = b;
	record_buffer->value_ballot = b;
	record_buffer->is_final = 1;
	record_buffer->value_size = size;
	memcpy(record_buffer->value, value, size);
	
	return record_buffer;
}

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

struct mem_storage
{
	int acceptor_id;
	paxos_accepted *records[MAX_RECORDS];
};

static void paxos_accepted_copy(paxos_accepted* dst, paxos_accepted* src);

static struct mem_storage*
mem_storage_new(int acceptor_id)
{
	int i;
	struct mem_storage* s = malloc(sizeof(struct mem_storage));
	memset(s, 0, sizeof(struct mem_storage));
	for (i = 0; i < MAX_RECORDS; ++i) {
		s->records[i] = malloc(sizeof(paxos_accepted));
		memset(s->records[i], 0, sizeof(paxos_accepted));
	}
	s->acceptor_id = acceptor_id;
	return s;
}

static int
mem_storage_open(void* handle)
{
	return 1;
}

static int
mem_storage_close(void* handle)
{
	int i;
	struct mem_storage* s = handle;
	for (i = 0; i < MAX_RECORDS; ++i) {
		if (s->records[i]->value.paxos_value_val != NULL)
			free(s->records[i]->value.paxos_value_val);
		free(s->records[i]);
	}
	free(s);
	return 0;
}

static void
mem_storage_tx_begin(void* handle)
{
	return;
}

static void
mem_storage_tx_commit(void* handle)
{
	return;
}

static int
mem_storage_get(void* handle, iid_t iid, paxos_accepted* out)
{
	struct mem_storage* s = handle;
	paxos_accepted* record = s->records[iid % MAX_RECORDS];
	if (iid < record->iid) {
		paxos_log_error("Instance %d too old! Current is %d", iid, record->iid);
		exit(1);
	}
	if (iid == record->iid)
		paxos_accepted_copy(out, record);
	else
		return 0;
	return 1;
}

static int
mem_storage_put(void* handle, paxos_accepted* acc)
{
	struct mem_storage* s = handle;
	paxos_accepted* record = s->records[acc->iid % MAX_RECORDS];
	paxos_accepted_destroy(record);
	paxos_accepted_copy(record, acc);
	return 1;
}

static void
paxos_accepted_copy(paxos_accepted* dst, paxos_accepted* src)
{
	memcpy(dst, src, sizeof(paxos_accepted));
	if (dst->value.paxos_value_len > 0) {
		dst->value.paxos_value_val = malloc(src->value.paxos_value_len);
		memcpy(dst->value.paxos_value_val, src->value.paxos_value_val,
			src->value.paxos_value_len);
	}
}

void
storage_init_mem(struct storage* s, int acceptor_id)
{
	s->handle = mem_storage_new(acceptor_id);
	s->api.open = mem_storage_open;
	s->api.close = mem_storage_close;
	s->api.tx_begin = mem_storage_tx_begin;
	s->api.tx_commit = mem_storage_tx_commit;
	s->api.get = mem_storage_get;
	s->api.put = mem_storage_put;
}

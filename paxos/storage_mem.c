/*
 * Copyright (c) 2013-2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "storage.h"
#include "khash.h"

KHASH_MAP_INIT_INT(record, paxos_accepted*);

struct mem_storage
{
	iid_t trim_iid;
	kh_record_t* records;
};

static void paxos_accepted_copy(paxos_accepted* dst, paxos_accepted* src);

static struct mem_storage*
mem_storage_new(int acceptor_id)
{
	struct mem_storage* s = malloc(sizeof(struct mem_storage));
	if (s == NULL)
		return s;
	s->trim_iid = 0;
	s->records = kh_init(record);
	return s;
}

static int
mem_storage_open(void* handle)
{
	return 0;
}

static void
mem_storage_close(void* handle)
{
	struct mem_storage* s = handle;
	paxos_accepted* accepted;
	kh_foreach_value(s->records, accepted, paxos_accepted_free(accepted));
	kh_destroy_record(s->records);
	free(s);
}

static int
mem_storage_tx_begin(void* handle)
{
	return 0;
}

static int
mem_storage_tx_commit(void* handle)
{
	return 0;
}

static void
mem_storage_tx_abort(void* handle) { }

static int
mem_storage_get(void* handle, iid_t iid, paxos_accepted* out)
{
	khiter_t k;
	struct mem_storage* s = handle;
	k = kh_get_record(s->records, iid);
	if (k == kh_end(s->records))
		return 0;
	paxos_accepted_copy(out, kh_value(s->records, k));
	return 1;
}

static int
mem_storage_put(void* handle, paxos_accepted* acc)
{
	int rv;
	khiter_t k;
	struct mem_storage* s = handle;
	paxos_accepted* record = malloc(sizeof(paxos_accepted));
	paxos_accepted_copy(record, acc);
	k = kh_put_record(s->records, acc->iid, &rv);
	if (rv == -1) { // error
		free(record);
		return -1;
	}
	if (rv == 0) { // key is already present
		paxos_accepted_free(kh_value(s->records, k));
	}
	kh_value(s->records, k) = record;
	return 0;
}

static int
mem_storage_trim(void* handle, iid_t iid)
{
	khiter_t k;
	struct mem_storage* s = handle;
	for (k = kh_begin(s->records); k != kh_end(s->records); ++k) {
		if (kh_exist(s->records, k)) {
			paxos_accepted* acc = kh_value(s->records, k);
			if (acc->iid <= iid) {
				paxos_accepted_free(acc);
				kh_del_record(s->records, k);
			}
		}
	}
	s->trim_iid = iid;
	return 0;
}

static iid_t
mem_storage_get_trim_instance(void* handle)
{
	struct mem_storage* s = handle;
	return s->trim_iid;
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
	s->api.tx_abort = mem_storage_tx_abort;
	s->api.get = mem_storage_get;
	s->api.put = mem_storage_put;
	s->api.trim = mem_storage_trim;
	s->api.get_trim_instance = mem_storage_get_trim_instance;
}

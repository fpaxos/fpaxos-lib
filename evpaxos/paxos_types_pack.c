/*
 * Copyright (c) 2013-2015, University of Lugano
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


#include "paxos_types_pack.h"

#define MSGPACK_OBJECT_AT(obj, i) (obj->via.array.ptr[i].via)


static void msgpack_pack_string(msgpack_packer* p, char* buffer, int len)
{
	#if MSGPACK_VERSION_MAJOR > 0
	msgpack_pack_bin(p, len);
	msgpack_pack_bin_body(p, buffer, len);
	#else
	msgpack_pack_raw(p, len);
	msgpack_pack_raw_body(p, buffer, len);
	#endif
}

static void msgpack_unpack_uint32_at(msgpack_object* o, uint32_t* v, int* i)
{
	*v = (uint32_t)MSGPACK_OBJECT_AT(o,*i).u64;
	(*i)++;
}

static void msgpack_unpack_string_at(msgpack_object* o, char** buffer, int* len, int* i)
{
	*buffer = NULL;
	#if MSGPACK_VERSION_MAJOR > 0
	*len = MSGPACK_OBJECT_AT(o,*i).bin.size;
	const char* obj = MSGPACK_OBJECT_AT(o,*i).bin.ptr;
	#else
	*len = MSGPACK_OBJECT_AT(o,*i).raw.size;
	const char* obj = MSGPACK_OBJECT_AT(o,*i).raw.ptr;
	#endif
	if (*len > 0) {
		*buffer = malloc(*len);
		memcpy(*buffer, obj, *len);
	}
	(*i)++;
}

static void msgpack_pack_paxos_value(msgpack_packer* p, paxos_value* v)
{
	msgpack_pack_string(p, v->paxos_value_val, v->paxos_value_len);
}

static void msgpack_unpack_paxos_value_at(msgpack_object* o, paxos_value* v, int* i)
{
	msgpack_unpack_string_at(o, &v->paxos_value_val, &v->paxos_value_len, i);
}

void msgpack_pack_paxos_prepare(msgpack_packer* p, paxos_prepare* v)
{
	msgpack_pack_array(p, 3);
	msgpack_pack_int32(p, PAXOS_PREPARE);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
}

void msgpack_unpack_paxos_prepare(msgpack_object* o, paxos_prepare* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_uint32_at(o, &v->ballot, &i);
}

void msgpack_pack_paxos_promise(msgpack_packer* p, paxos_promise* v)
{
	msgpack_pack_array(p, 6);
	msgpack_pack_int32(p, PAXOS_PROMISE);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
	msgpack_pack_uint32(p, v->value_ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_promise(msgpack_object* o, paxos_promise* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_uint32_at(o, &v->ballot, &i);
	msgpack_unpack_uint32_at(o, &v->value_ballot, &i);
	msgpack_unpack_paxos_value_at(o, &v->value, &i);
}

void msgpack_pack_paxos_accept(msgpack_packer* p, paxos_accept* v)
{
	msgpack_pack_array(p, 4);
	msgpack_pack_int32(p, PAXOS_ACCEPT);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_accept(msgpack_object* o, paxos_accept* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_uint32_at(o, &v->ballot, &i);
	msgpack_unpack_paxos_value_at(o, &v->value, &i);
}

void msgpack_pack_paxos_accepted(msgpack_packer* p, paxos_accepted* v)
{
	msgpack_pack_array(p, 6);
	msgpack_pack_int32(p, PAXOS_ACCEPTED);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
	msgpack_pack_uint32(p, v->value_ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_accepted(msgpack_object* o, paxos_accepted* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_uint32_at(o, &v->ballot, &i);
	msgpack_unpack_uint32_at(o, &v->value_ballot, &i);
	msgpack_unpack_paxos_value_at(o, &v->value, &i);
}

void msgpack_pack_paxos_preempted(msgpack_packer* p, paxos_preempted* v)
{
	msgpack_pack_array(p, 4);
	msgpack_pack_int32(p, PAXOS_PREEMPTED);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
}

void msgpack_unpack_paxos_preempted(msgpack_object* o, paxos_preempted* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_uint32_at(o, &v->ballot, &i);
}

void msgpack_pack_paxos_repeat(msgpack_packer* p, paxos_repeat* v)
{
	msgpack_pack_array(p, 3);
	msgpack_pack_int32(p, PAXOS_REPEAT);
	msgpack_pack_uint32(p, v->from);
	msgpack_pack_uint32(p, v->to);
}

void msgpack_unpack_paxos_repeat(msgpack_object* o, paxos_repeat* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->from, &i);
	msgpack_unpack_uint32_at(o, &v->to, &i);
}

void msgpack_pack_paxos_trim(msgpack_packer* p, paxos_trim* v)
{
	msgpack_pack_array(p, 2);
	msgpack_pack_int32(p, PAXOS_TRIM);
	msgpack_pack_uint32(p, v->iid);
}

void msgpack_unpack_paxos_trim(msgpack_object* o, paxos_trim* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->iid, &i);
}

void msgpack_pack_paxos_acceptor_state(msgpack_packer* p, paxos_acceptor_state* v)
{
	msgpack_pack_array(p, 3);
	msgpack_pack_int32(p, PAXOS_ACCEPTOR_STATE);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->trim_iid);
}

void msgpack_unpack_paxos_acceptor_state(msgpack_object* o, paxos_acceptor_state* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->trim_iid, &i);
}

void msgpack_pack_paxos_client_value(msgpack_packer* p, paxos_client_value* v)
{
	msgpack_pack_array(p, 2);
	msgpack_pack_int32(p, PAXOS_CLIENT_VALUE);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_client_value(msgpack_object* o, paxos_client_value* v)
{
	int i = 1;
	msgpack_unpack_paxos_value_at(o, &v->value, &i);
}

void msgpack_pack_paxos_message(msgpack_packer* p, paxos_message* v)
{
	switch (v->type) {
	case PAXOS_PREPARE:
		msgpack_pack_paxos_prepare(p, &v->u.prepare);
		break;
	case PAXOS_PROMISE:
		msgpack_pack_paxos_promise(p, &v->u.promise);
		break;
	case PAXOS_ACCEPT:
		msgpack_pack_paxos_accept(p, &v->u.accept);
		break;
	case PAXOS_ACCEPTED:
		msgpack_pack_paxos_accepted(p, &v->u.accepted);
		break;
	case PAXOS_PREEMPTED:
		msgpack_pack_paxos_preempted(p, &v->u.preempted);
		break;
	case PAXOS_REPEAT:
		msgpack_pack_paxos_repeat(p, &v->u.repeat);
		break;
	case PAXOS_TRIM:
		msgpack_pack_paxos_trim(p, &v->u.trim);
		break;
	case PAXOS_ACCEPTOR_STATE:
		msgpack_pack_paxos_acceptor_state(p, &v->u.state);
		break;
	case PAXOS_CLIENT_VALUE:
		msgpack_pack_paxos_client_value(p, &v->u.client_value);
		break;
	}
}

void msgpack_unpack_paxos_message(msgpack_object* o, paxos_message* v)
{
	v->type = MSGPACK_OBJECT_AT(o,0).u64;
	switch (v->type) {
	case PAXOS_PREPARE:
		msgpack_unpack_paxos_prepare(o, &v->u.prepare);
		break;
	case PAXOS_PROMISE:
		msgpack_unpack_paxos_promise(o, &v->u.promise);
		break;
	case PAXOS_ACCEPT:
		msgpack_unpack_paxos_accept(o, &v->u.accept);
		break;
	case PAXOS_ACCEPTED:
		msgpack_unpack_paxos_accepted(o, &v->u.accepted);
		break;
	case PAXOS_PREEMPTED:
		msgpack_unpack_paxos_preempted(o, &v->u.preempted);
		break;
	case PAXOS_REPEAT:
		msgpack_unpack_paxos_repeat(o, &v->u.repeat);
		break;
	case PAXOS_TRIM:
		msgpack_unpack_paxos_trim(o, &v->u.trim);
		break;
	case PAXOS_ACCEPTOR_STATE:
		msgpack_unpack_paxos_acceptor_state(o, &v->u.state);
		break;
	case PAXOS_CLIENT_VALUE:
		msgpack_unpack_paxos_client_value(o, &v->u.client_value);
		break;
	}
}

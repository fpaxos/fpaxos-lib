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


#include "paxos_types_pack.h"

#define UNPACK_ARRAY_AT(obj, i) obj->via.array.ptr[i].via


static void unpack_raw_at(msgpack_object* o, paxos_value* v, int i)
{
	int len = UNPACK_ARRAY_AT(o,i).raw.size;
	v->paxos_value_val = NULL;
	if (len > 0) {
		v->paxos_value_val = malloc(len);
		memcpy(v->paxos_value_val, UNPACK_ARRAY_AT(o,i).raw.ptr, len);
	}
	v->paxos_value_len = len;
}

static void unpack_uint32_at(msgpack_object* o, uint32_t* u, int i)
{
	*u = UNPACK_ARRAY_AT(o,i).u64;
}

void msgpack_pack_paxos_value(msgpack_packer* p, paxos_value* v)
{
	msgpack_pack_raw(p, v->paxos_value_len);
	msgpack_pack_raw_body(p, v->paxos_value_val, v->paxos_value_len);
}

void msgpack_pack_paxos_prepare(msgpack_packer* p, paxos_prepare* v)
{
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
}

void msgpack_unpack_paxos_prepare(msgpack_object* o, paxos_prepare* v)
{
	unpack_uint32_at(o, &v->iid, 1);
	unpack_uint32_at(o, &v->ballot, 2);
}

void msgpack_pack_paxos_promise(msgpack_packer* p, paxos_promise* v)
{
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
	msgpack_pack_uint32(p, v->value_ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_promise(msgpack_object* o, paxos_promise* v)
{
	unpack_uint32_at(o, &v->iid, 1);
	unpack_uint32_at(o, &v->ballot, 2);
	unpack_uint32_at(o, &v->value_ballot, 3);
	unpack_raw_at(o, &v->value, 4);
}

void msgpack_pack_paxos_accept(msgpack_packer* p, paxos_accept* v)
{
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_accept(msgpack_object* o, paxos_accept* v)
{
	unpack_uint32_at(o, &v->iid, 1);
	unpack_uint32_at(o, &v->ballot, 2);
	unpack_raw_at(o, &v->value, 3);
}

void msgpack_pack_paxos_accepted(msgpack_packer* p, paxos_accepted* v)
{
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_uint32(p, v->ballot);
	msgpack_pack_uint32(p, v->value_ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_accepted(msgpack_object* o, paxos_accepted* v)
{
	unpack_uint32_at(o, &v->iid, 1);
	unpack_uint32_at(o, &v->ballot, 2);
	unpack_uint32_at(o, &v->value_ballot, 3);
	unpack_raw_at(o, &v->value, 4);
}

void msgpack_pack_paxos_repeat(msgpack_packer* p, paxos_repeat* v)
{
	msgpack_pack_uint32(p, v->from);
	msgpack_pack_uint32(p, v->to);
}

void msgpack_unpack_paxos_repeat(msgpack_object* o, paxos_repeat* v)
{
	unpack_uint32_at(o, &v->from, 1);
	unpack_uint32_at(o, &v->to, 2);
}

void msgpack_pack_paxos_client_value(msgpack_packer* p, paxos_client_value* v)
{
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_client_value(msgpack_object* o, paxos_client_value* v)
{
	unpack_raw_at(o, &v->value, 1);
}

void msgpack_pack_paxos_message(msgpack_packer* p, paxos_message* v)
{
	switch (v->type) {
	case PAXOS_PREPARE:
		msgpack_pack_array(p, 3);
		msgpack_pack_int(p, PAXOS_PREPARE);
		msgpack_pack_paxos_prepare(p, &v->u.prepare);
		break;
	case PAXOS_PROMISE:
		msgpack_pack_array(p, 5);
		msgpack_pack_int(p, PAXOS_PROMISE);
		msgpack_pack_paxos_promise(p, &v->u.promise);
		break;
	case PAXOS_ACCEPT:
		msgpack_pack_array(p, 4);
		msgpack_pack_int(p, PAXOS_ACCEPT);
		msgpack_pack_paxos_accept(p, &v->u.accept);
		break;
	case PAXOS_ACCEPTED:
		msgpack_pack_array(p, 5);
		msgpack_pack_int(p, PAXOS_ACCEPTED);
		msgpack_pack_paxos_accepted(p, &v->u.accepted);
		break;
	case PAXOS_REPEAT:
		msgpack_pack_array(p, 3);
		msgpack_pack_int(p, PAXOS_REPEAT);
		msgpack_pack_paxos_repeat(p, &v->u.repeat);
		break;
	case PAXOS_CLIENT_VALUE:
		msgpack_pack_array(p, 2);
		msgpack_pack_int(p, PAXOS_CLIENT_VALUE);
		msgpack_pack_paxos_client_value(p, &v->u.client_value);
		break;
	}
}

void msgpack_unpack_paxos_message(msgpack_object* o, paxos_message* v)
{
	unpack_uint32_at(o, &v->type, 0);
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
	case PAXOS_REPEAT:
		msgpack_unpack_paxos_repeat(o, &v->u.repeat);
		break;
	case PAXOS_CLIENT_VALUE:
		msgpack_unpack_paxos_client_value(o, &v->u.client_value);
		break;
	}
}

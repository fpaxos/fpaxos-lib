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


#ifndef _PAXOS_TYPES_PACK_H_
#define _PAXOS_TYPES_PACK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "paxos_types.h"
#include <msgpack.h>

void msgpack_pack_paxos_prepare(msgpack_packer* p, paxos_prepare* v);
void msgpack_unpack_paxos_prepare(msgpack_object* o, paxos_prepare* v);
void msgpack_pack_paxos_promise(msgpack_packer* p, paxos_promise* v);
void msgpack_unpack_paxos_promise(msgpack_object* o, paxos_promise* v);
void msgpack_pack_paxos_accept(msgpack_packer* p, paxos_accept* v);
void msgpack_unpack_paxos_accept(msgpack_object* o, paxos_accept* v);
void msgpack_pack_paxos_accepted(msgpack_packer* p, paxos_accepted* v);
void msgpack_unpack_paxos_accepted(msgpack_object* o, paxos_accepted* v);
void msgpack_pack_paxos_preempted(msgpack_packer* p, paxos_preempted* v);
void msgpack_unpack_paxos_preempted(msgpack_object* o, paxos_preempted* v);
void msgpack_pack_paxos_repeat(msgpack_packer* p, paxos_repeat* v);
void msgpack_unpack_paxos_repeat(msgpack_object* o, paxos_repeat* v);
void msgpack_pack_paxos_trim(msgpack_packer* p, paxos_trim* v);
void msgpack_unpack_paxos_trim(msgpack_object* o, paxos_trim* v);
void msgpack_pack_paxos_acceptor_state(msgpack_packer* p, paxos_acceptor_state* v);
void msgpack_unpack_paxos_acceptor_state(msgpack_object* o, paxos_acceptor_state* v);
void msgpack_pack_paxos_client_value(msgpack_packer* p, paxos_client_value* v);
void msgpack_unpack_paxos_client_value(msgpack_object* o, paxos_client_value* v);
void msgpack_pack_paxos_message(msgpack_packer* p, paxos_message* v);
void msgpack_unpack_paxos_message(msgpack_object* o, paxos_message* v);

#ifdef __cplusplus
}
#endif

#endif

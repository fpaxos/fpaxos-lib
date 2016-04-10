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


#ifndef _TCP_SENDBUF_H_
#define _TCP_SENDBUF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "paxos_types.h"
#include <event2/buffer.h>
#include <event2/bufferevent.h>

void send_paxos_message(struct bufferevent* bev, paxos_message* msg);
void send_paxos_prepare(struct bufferevent* bev, paxos_prepare* msg);
void send_paxos_promise(struct bufferevent* bev, paxos_promise* msg);
void send_paxos_accept(struct bufferevent* bev, paxos_accept* msg);
void send_paxos_accepted(struct bufferevent* bev, paxos_accepted* msg);
void send_paxos_preempted(struct bufferevent* bev, paxos_preempted* msg);
void send_paxos_repeat(struct bufferevent* bev, paxos_repeat* msg);
void send_paxos_trim(struct bufferevent* bev, paxos_trim* msg);
int recv_paxos_message(struct evbuffer* in, paxos_message* out);

#ifdef __cplusplus
}
#endif

#endif

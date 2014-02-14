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


#include "evpaxos.h"
#include "peers.h"
#include "config.h"
#include "acceptor.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <event2/event.h>

struct evacceptor
{
	int acceptor_id;
	struct acceptor* state;
	struct event_base* base;
	struct peers* peers;
	struct evpaxos_config* conf;
};


static void
peer_send_accepted(struct peer* p, void* arg)
{
	send_paxos_accepted(peer_get_buffer(p), arg);
}

/*
	Received a prepare request (phase 1a).
*/
static void 
evacceptor_handle_prepare(struct evacceptor* a, 
	struct peer* p, paxos_prepare* m)
{
	paxos_promise promise;
	paxos_log_debug("Handle prepare for iid %d ballot %d", m->iid, m->ballot);
	acceptor_receive_prepare(a->state, m, &promise);
	send_paxos_promise(peer_get_buffer(p), &promise);
	paxos_promise_destroy(&promise);
}

/*
	Received a accept request (phase 2a).
*/
static void 
evacceptor_handle_accept(struct evacceptor* a, struct peer* p, 
	paxos_accept* accept)
{	
	paxos_accepted accepted;
	paxos_log_debug("Handle accept for iid %d bal %d", 
		accept->iid, accept->ballot);
	acceptor_receive_accept(a->state, accept, &accepted);
	if (accept->ballot == accepted.ballot) // accepted!
		peers_foreach_client(a->peers, peer_send_accepted, &accepted);
	else // send nack
		send_paxos_accepted(peer_get_buffer(p), &accepted);
	paxos_accepted_destroy(&accepted);
}

static void
evacceptor_handle_repeat(struct evacceptor* a,
	struct peer* p, paxos_repeat* repeat)
{	
	iid_t iid;
	paxos_accepted accepted;
	paxos_log_debug("Handle repeat for iids %d-%d", repeat->from, repeat->to);
	for (iid = repeat->from; iid <= repeat->to; ++iid) {
		if (acceptor_receive_repeat(a->state, iid, &accepted)) {
			send_paxos_accepted(peer_get_buffer(p), &accepted);
			paxos_accepted_destroy(&accepted);
		}
	}
}

/*
	This function is invoked when a new paxos message has been received.
*/
static void 
evacceptor_handle_msg(struct peer* p, paxos_message* msg, void* arg)
{
	struct evacceptor* a = (struct evacceptor*)arg;
	switch (msg->type) {
		case PAXOS_PREPARE:
			evacceptor_handle_prepare(a, p, &msg->u.prepare);
			break;
		case PAXOS_ACCEPT:
			evacceptor_handle_accept(a, p, &msg->u.accept);
			break;
		case PAXOS_REPEAT:
			evacceptor_handle_repeat(a, p, &msg->u.repeat);
			break;
		default:
			paxos_log_error("Unknow msg type %d not handled", msg->type);
	}
}

struct evacceptor* 
evacceptor_init(int id, const char* config_file, struct event_base* b)
{
	int port;
	int acceptor_count;
	struct evacceptor* a;
	
	a = malloc(sizeof(struct evacceptor));

	a->conf = evpaxos_config_read(config_file);
	if (a->conf == NULL) {
		free(a);
		return NULL;
	}
	
	port = evpaxos_acceptor_listen_port(a->conf, id);
	acceptor_count = evpaxos_acceptor_count(a->conf);
	
	if (id < 0 || id >= acceptor_count) {
		paxos_log_error("Invalid acceptor id: %d.", id);
		paxos_log_error("Should be between 0 and %d", acceptor_count);
		return NULL;
	}
	
    a->acceptor_id = id;
	a->base = b;
	a->peers = peers_new(a->base, a->conf, evacceptor_handle_msg, a);
	a->state = acceptor_new(id);
	
	int rv = peers_listen(a->peers, port);
	if (rv == 0)
		return NULL;

    return a;
}

void
evacceptor_free(struct evacceptor* a)
{
	acceptor_free(a->state);
	peers_free(a->peers);
	evpaxos_config_free(a->conf);
	free(a);
}

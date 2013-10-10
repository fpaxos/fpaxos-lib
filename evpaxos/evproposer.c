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
#include "message.h"
#include "tcp_receiver.h"
#include "proposer.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

struct evproposer
{
	int id;
	int preexec_window;
	struct tcp_receiver* receiver;
	struct event_base* base;
	struct proposer* state;
	struct peers* acceptors;
	struct timeval tv;
	struct event* timeout_ev;
};


static void
send_prepares(struct evproposer* p, paxos_prepare* pr)
{
	int i;
	for (i = 0; i < peers_count(p->acceptors); i++) {
		struct bufferevent* bev = peers_get_buffer(p->acceptors, i);
		send_paxos_prepare(bev, pr);
	}
}

static void
send_accepts(struct evproposer* p, paxos_accept* ar)
{
	int i;
	for (i = 0; i < peers_count(p->acceptors); i++) {
		struct bufferevent* bev = peers_get_buffer(p->acceptors, i);
    	send_paxos_accept(bev, ar);
	}
}

static void
proposer_preexecute(struct evproposer* p)
{
	int i;
	paxos_prepare pr;
	int count = p->preexec_window - proposer_prepared_count(p->state);
	if (count <= 0) return;
	for (i = 0; i < count; i++) {
		proposer_prepare(p->state, &pr);
		send_prepares(p, &pr);
	}
	paxos_log_debug("Opened %d new instances", count);
}

static void
try_accept(struct evproposer* p)
{
	paxos_accept accept;
	while (proposer_accept(p->state, &accept))
		send_accepts(p, &accept);
	proposer_preexecute(p);
}

static void
evproposer_handle_promise(struct evproposer* p, paxos_promise* promise,
	int from)
{
	int preempted;
	paxos_prepare prepare;
	preempted = proposer_receive_promise(p->state, promise, from, &prepare);
	if (preempted)
		send_prepares(p, &prepare);
}

static void
evproposer_handle_accepted(struct evproposer* p, paxos_accepted* accepted,
	 int from)
{
	int preempted;
	paxos_prepare prepare;
	preempted = proposer_receive_accepted(p->state, accepted, from, &prepare);
	if (preempted)
		send_prepares(p, &prepare);
}

static void
evproposer_handle_client_value(struct evproposer* p, paxos_client_value* v)
{
	proposer_propose(p->state,
		v->value.paxos_value_val, 
		v->value.paxos_value_len);
}

static void
evproposer_handle_acceptor_msg(paxos_message* msg, int from, void* arg)
{
	struct evproposer* p = arg;
	switch (msg->type) {
		case PAXOS_PROMISE:
			evproposer_handle_promise(p, &msg->paxos_message_u.promise, from);
			break;
		case PAXOS_ACCEPTED:
			evproposer_handle_accepted(p, &msg->paxos_message_u.accepted, from);
			break;
		default:
			paxos_log_error("Unknow msg type %d not handled", msg->type);
			return;
	}
	try_accept(p);
}

static void
evproposer_handle_client_msg(struct bufferevent* bev, paxos_message* msg,
	void* arg)
{
	struct evproposer* p = arg;
	switch (msg->type) {
		case PAXOS_CLIENT_VALUE:
			evproposer_handle_client_value(p, 	
				&msg->paxos_message_u.client_value);
			break;
		default:
			paxos_log_error("Unknow msg type %d not handled", msg->type);
			return;
	}
	try_accept(p);
}


static void
evproposer_check_timeouts(evutil_socket_t fd, short event, void *arg)
{
	struct evproposer* p = arg;
	struct timeout_iterator* iter = proposer_timeout_iterator(p->state);

	paxos_prepare pr;	
	while (timeout_iterator_prepare(iter, &pr)) {
		paxos_log_info("Instance %d timed out.", pr.iid);
		send_prepares(p, &pr);
	}
	
	paxos_accept ar;
	while (timeout_iterator_accept(iter, &ar)) {
		paxos_log_info("Instance %d timed out.", ar.iid);
		send_accepts(p, &ar);
	}
	
	timeout_iterator_free(iter);
	event_add(p->timeout_ev, &p->tv);
}

struct evproposer*
evproposer_init(int id, const char* config_file, struct event_base* b)
{
	int port;
	int acceptor_count;
	struct evproposer* p;
	struct evpaxos_config* conf = evpaxos_config_read(config_file);
	
	if (conf == NULL)
		return NULL;
	
	// Check id validity of proposer_id
	if (id < 0 || id >= MAX_N_OF_PROPOSERS) {
		paxos_log_error("Invalid proposer id: %d", id);
		return NULL;
	}

	port = evpaxos_proposer_listen_port(conf, id);
	acceptor_count = evpaxos_acceptor_count(conf);
	
	p = malloc(sizeof(struct evproposer));
	p->id = id;
	p->base = b;
	p->preexec_window = paxos_config.proposer_preexec_window;
		
	// Setup client listener
	p->receiver = tcp_receiver_new(b, port, evproposer_handle_client_msg, p);
	
	// Setup connections to acceptors
	p->acceptors = peers_new(b, conf);
	peers_connect_to_acceptors(p->acceptors, evproposer_handle_acceptor_msg, p);
	
	// Setup timeout
	p->tv.tv_sec = paxos_config.proposer_timeout;
	p->tv.tv_usec = 0;
	p->timeout_ev = evtimer_new(b, evproposer_check_timeouts, p);
	event_add(p->timeout_ev, &p->tv);
	
	p->state = proposer_new(p->id, acceptor_count);
	
	proposer_preexecute(p);
	
	evpaxos_config_free(conf);
	return p;
}

void
evproposer_free(struct evproposer* p)
{
	peers_free(p->acceptors);
	tcp_receiver_free(p->receiver);
	event_free(p->timeout_ev);
	proposer_free(p->state);
	free(p);
}

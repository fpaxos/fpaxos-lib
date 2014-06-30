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
#include "libpaxos_messages.h"
#include "tcp_sendbuf.h"
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
send_prepares(struct evproposer* p, prepare_req* pr)
{
	int i;
	for (i = 0; i < peers_count(p->acceptors); i++) {
		struct bufferevent* bev = peers_get_buffer(p->acceptors, i);
		sendbuf_add_prepare_req(bev, pr);
	}
}

static void
send_accepts(struct evproposer* p, accept_req* ar)
{
	int i;
	for (i = 0; i < peers_count(p->acceptors); i++) {
		struct bufferevent* bev = peers_get_buffer(p->acceptors, i);
    	sendbuf_add_accept_req(bev, ar);
	}
}

static void
proposer_preexecute(struct evproposer* p)
{
	int i;
	prepare_req pr;
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
	accept_req* ar;
	while ((ar = proposer_accept(p->state)) != NULL) {
		send_accepts(p, ar);
		free(ar);
	}
	proposer_preexecute(p);
}

static void
proposer_handle_prepare_ack(struct evproposer* p, prepare_ack* ack)
{
	prepare_req pr;
	int preempted = proposer_receive_prepare_ack(p->state, ack, &pr);
	if (preempted)
		send_prepares(p, &pr);
}

static void
proposer_handle_accept_ack(struct evproposer* p, accept_ack* ack)
{
	prepare_req pr;
	int preempted = proposer_receive_accept_ack(p->state, ack, &pr);
	if (preempted)
		send_prepares(p, &pr);
}

static void
proposer_handle_client_msg(struct evproposer* p, char* value, int size)
{
	proposer_propose(p->state, value, size);
}

static void
proposer_handle_msg(struct evproposer* p, struct bufferevent* bev)
{
	paxos_msg msg;
	struct evbuffer* in;
	char* buffer = NULL;

	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));

	if (msg.data_size > 0) {
		buffer = malloc(msg.data_size);
		evbuffer_remove(in, buffer, msg.data_size);
	}

	switch (msg.type) {
		case prepare_acks:
			proposer_handle_prepare_ack(p, (prepare_ack*)buffer);
			break;
		case accept_acks:
			proposer_handle_accept_ack(p, (accept_ack*)buffer);
			break;
		case submit:
			proposer_handle_client_msg(p, buffer, msg.data_size);
			break;
		default:
			paxos_log_error("Unknow msg type %d not handled", msg.type);
			return;
	}
	
	try_accept(p);
	if (buffer != NULL)
		free(buffer);
}

static void
handle_request(struct bufferevent* bev, void* arg)
{
	size_t len;
	paxos_msg msg;
	struct evproposer* p = arg;
	struct evbuffer* in = bufferevent_get_input(bev);
	
	while ((len = evbuffer_get_length(in)) > sizeof(paxos_msg)) {
		evbuffer_copyout(in, &msg, sizeof(paxos_msg));
		if (len < PAXOS_MSG_SIZE((&msg)))
			return;
		proposer_handle_msg(p, bev);
	}
}

static void
proposer_check_timeouts(evutil_socket_t fd, short event, void *arg)
{
	struct evproposer* p = arg;
	struct timeout_iterator* iter = proposer_timeout_iterator(p->state);
	
	prepare_req* pr;
	while ((pr = timeout_iterator_prepare(iter)) != NULL) {
		paxos_log_info("Instance %d timed out.", pr->iid);
		send_prepares(p, pr);
		free(pr);
	}
	
	accept_req* ar;
	while ((ar = timeout_iterator_accept(iter)) != NULL) {
		paxos_log_info("Instance %d timed out.", ar->iid);
		send_accepts(p, ar);
		free(ar);
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
	p->receiver = tcp_receiver_new(b, port, handle_request, p);
	
	// Setup connections to acceptors
	p->acceptors = peers_new(b);
	peers_connect_to_acceptors(p->acceptors, conf, handle_request, p);
	
	// Setup timeout
	p->tv.tv_sec = paxos_config.proposer_timeout;
	p->tv.tv_usec = 0;
	p->timeout_ev = evtimer_new(b, proposer_check_timeouts, p);
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
	proposer_free(p->state);
	free(p);
}

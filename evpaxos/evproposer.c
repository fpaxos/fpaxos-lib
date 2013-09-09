/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "evpaxos.h"
#include "peers.h"
#include "config.h"
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
evproposer_handle_promise(struct evproposer* p, paxos_promise* ack)
{
	paxos_prepare pr;
	int preempted = proposer_receive_promise(p->state, ack, &pr);
	if (preempted)
		send_prepares(p, &pr);
}

static void
evproposer_handle_accepted(struct evproposer* p, paxos_accepted* ack)
{
	paxos_prepare pr;
	int preempted = proposer_receive_accepted(p->state, ack, &pr);
	if (preempted)
		send_prepares(p, &pr);
}

static void
evproposer_handle_client_value(struct evproposer* p, paxos_client_value* v)
{
	proposer_propose(p->state, v->value.value_val, v->value.value_len);
}

static void
evproposer_handle_msg(struct bufferevent* bev, paxos_message* msg, void* arg)
{
	struct evproposer* p = arg;
	switch (msg->type) {
		case PAXOS_PROMISE:
			evproposer_handle_promise(p, &msg->paxos_message_u.promise);
			break;
		case PAXOS_ACCEPTED:
			evproposer_handle_accepted(p, &msg->paxos_message_u.accepted);
			break;
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
	p->receiver = tcp_receiver_new(b, port, evproposer_handle_msg, p);
	
	// Setup connections to acceptors
	p->acceptors = peers_new(b);
	peers_connect_to_acceptors(p->acceptors, conf, evproposer_handle_msg, p);
	
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

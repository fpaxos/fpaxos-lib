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
#include "config_reader.h"
#include "tcp_sendbuf.h"
#include "tcp_receiver.h"
#include "proposer.h"
#include "paxos_config.h"
#include <assert.h>
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
	int acceptors_count;
	struct bufferevent* acceptor_ev[N_OF_ACCEPTORS];
	struct tcp_receiver* receiver;
	struct event_base* base;
	struct proposer* state;
};


static void
do_prepare(struct evproposer* p, prepare_req* pr)
{
	int i;
	for (i = 0; i < p->acceptors_count; i++)
		sendbuf_add_prepare_req(p->acceptor_ev[i], pr);
}

static void
proposer_preexecute(struct evproposer* p)
{
	int i;
	prepare_req pr;
	int count = p->preexec_window - proposer_prepared_count(p->state);
	if (count <= 0) return;
	for (i = 0; i < count; i++) {
		pr = proposer_prepare(p->state);
		do_prepare(p, &pr);
	}
	LOG(DBG, ("Opened %d new instances\n", count));
}

static void
try_accept(struct evproposer* p)
{
	int i;
	accept_req* ar;
	while ((ar = proposer_accept(p->state)) != NULL) {
		for (i = 0; i < p->acceptors_count; i++)
	    	sendbuf_add_accept_req(p->acceptor_ev[i], ar);
		free(ar);
	}
	proposer_preexecute(p);
}

static void 
proposer_handle_prepare_ack(struct evproposer* p, prepare_ack* ack)
{
	prepare_req* pr = proposer_receive_prepare_ack(p->state, ack);
	if (pr != NULL) {
		do_prepare(p, pr);
		free(pr);
	}
	try_accept(p);
}

static void
proposer_handle_accept_ack(struct evproposer* p, accept_ack* ack)
{
	prepare_req* pr = proposer_receive_accept_ack(p->state, ack);
	if (pr != NULL) {
		do_prepare(p, pr);
		free(pr);
	}
	try_accept(p);
}

static void
proposer_handle_client_msg(struct evproposer* p, char* value, int size)
{
	proposer_propose(p->state, value, size);
	try_accept(p);
}

static void
proposer_handle_msg(struct evproposer* p, struct bufferevent* bev)
{
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];

	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	if (msg.data_size > PAXOS_MAX_VALUE_SIZE)
	{
		evbuffer_drain(in, msg.data_size);
		LOG(VRB, ("Proposer received req sz %d > %d maximum, discarding\n",
			msg.data_size, PAXOS_MAX_VALUE_SIZE));
		
		return;
	}
	evbuffer_remove(in, buffer, msg.data_size);
	
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
			LOG(VRB, ("Unknown msg type %d received from acceptors\n",
				msg.type));
	}
}

static void
handle_request(struct bufferevent* bev, void* arg)
{
	size_t len;
	paxos_msg msg;
	struct evbuffer* in;
	struct evproposer* p;
	
	p = arg;
	in = bufferevent_get_input(bev);
	
	while ((len = evbuffer_get_length(in)) > sizeof(paxos_msg)) {
		evbuffer_copyout(in, &msg, sizeof(paxos_msg));
		if (len < PAXOS_MSG_SIZE((&msg))) {
			LOG(DBG, ("not enough data\n"));
			return;
		}
		proposer_handle_msg(p, bev);
	}
}

static void
on_event(struct bufferevent *bev, short events, void *ptr)
{
	if (events & BEV_EVENT_CONNECTED) {
	LOG(VRB, ("Proposer connected...\n"));
	} else if (events & BEV_EVENT_ERROR) {
		LOG(VRB, ("Proposer connection error...\n"));
		bufferevent_disable(bev, EV_READ|EV_WRITE);
	}
}

static struct bufferevent* 
do_connect(struct evproposer* p, struct event_base* b, address* a) 
{
	struct sockaddr_in sin;
	struct bufferevent* bev;
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(a->address_string);
	sin.sin_port = htons(a->port);
	
	bev = bufferevent_socket_new(b, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, handle_request, NULL, on_event, p);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	struct sockaddr* addr = (struct sockaddr*)&sin;
	if (bufferevent_socket_connect(bev, addr, sizeof(sin)) < 0) {
		bufferevent_free(bev);
		return NULL;
	}
	return bev;
}

struct evproposer*
evproposer_init(int id, const char* config_file, struct event_base* b)
{
	int i;
	struct evproposer* p;
	
	struct config* conf = read_config(config_file);
	if (conf == NULL)
		return NULL;
	
	// Check id validity of proposer_id
	if (id < 0 || id >= MAX_N_OF_PROPOSERS) {
		printf("Invalid proposer id:%d\n", id);
		return NULL;
	}

	p = malloc(sizeof(struct evproposer));

	p->id = id;
	p->base = b;
	p->preexec_window = 128;
	p->acceptors_count = conf->acceptors_count;
	
    LOG(VRB, ("Proposer %d starting...\n", id));
		
	// Setup client listener
	p->receiver = tcp_receiver_new(b, &conf->proposers[id], handle_request, p);
	
	// Setup connections to acceptors
	for (i = 0; i < conf->acceptors_count; i++) {
		p->acceptor_ev[i] = do_connect(p, b, &conf->acceptors[i]);
		assert(p->acceptor_ev[i] != NULL);
	}
	
	p->state = proposer_new(p->id);
	proposer_preexecute(p);
	
	LOG(VRB, ("Proposer is ready\n"));
	return p;
}

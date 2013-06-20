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


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <memory.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "evpaxos.h"
#include "learner.h"
#include "peers.h"
#include "tcp_sendbuf.h"
#include "config_reader.h"

struct evlearner
{
	struct learner* state;      /* The actual learner */
	deliver_function delfun;    /* Delivery callback*/
	void* delarg;               /* The argument to the delivery callback */
	struct event* hole_timer;   /* Timer to check for holes */
	struct timeval tv;          /* Check for holes every tv units of time */
	struct peers* acceptors;    /* Connections to acceptors */
};


static void
learner_check_holes(evutil_socket_t fd, short event, void *arg)
{
	int i, chunks = 10000;
	iid_t iid, from, to;
	struct evlearner* l = arg;
	if (learner_has_holes(l->state, &from, &to)) {
		if ((to - from) > chunks)
			to = from + chunks;
		for (iid = from; iid < to; iid++) {
			for (i = 0; i < peers_count(l->acceptors); i++) {
				struct bufferevent* bev = peers_get_buffer(l->acceptors, i);
				sendbuf_add_repeat_req(bev, iid);
			}
		}
	}
	event_add(l->hole_timer, &l->tv);
}

static void 
learner_deliver_next_closed(struct evlearner* l)
{
	int prop_id;
	accept_ack* ack;
	while ((ack = learner_deliver_next(l->state)) != NULL) {
		// deliver the value through callback
		prop_id = ack->ballot % MAX_N_OF_PROPOSERS;
		l->delfun(ack->value, ack->value_size, ack->iid, 
			ack->ballot, prop_id, l->delarg);
		free(ack);
	}
}

/*
	Called when an accept_ack is received, the learner will update it's status
    for that instance and afterwards check if the instance is closed
*/
static void
learner_handle_accept_ack(struct evlearner* l, accept_ack * aa)
{
	learner_receive_accept(l->state, aa);
	learner_deliver_next_closed(l);
}

static void
learner_handle_msg(struct evlearner* l, struct bufferevent* bev)
{
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];

	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	if (msg.data_size > PAXOS_MAX_VALUE_SIZE) {
		evbuffer_drain(in, msg.data_size);
		LOG(VRB, ("Learner received req sz %ld > %d maximum, discarding\n",
			msg.data_size, PAXOS_MAX_VALUE_SIZE));
		return;
	}
	evbuffer_remove(in, buffer, msg.data_size);
	
	switch (msg.type) {
		case accept_acks:
			learner_handle_accept_ack(l, (accept_ack*)buffer);
			break;
		default:
			printf("Unknow msg type %d received from acceptors\n", msg.type);
    }
}

static void
on_acceptor_msg(struct bufferevent* bev, void* arg)
{
	size_t len;
	paxos_msg msg;
	struct evlearner* l = arg;
	struct evbuffer* in = bufferevent_get_input(bev);
	
	while ((len = evbuffer_get_length(in)) > sizeof(paxos_msg)) {
		evbuffer_copyout(in, &msg, sizeof(paxos_msg));
		if (len < PAXOS_MSG_SIZE((&msg))) {
			LOG(DBG, ("not enough data\n"));
			return;
		}
		learner_handle_msg(l, bev);
	}
}

struct evlearner*
evlearner_init_conf(struct config* c, deliver_function f, void* arg, 
	struct event_base* b)
{
	int i;
	struct evlearner* l;
	
	l = malloc(sizeof(struct evlearner));
	l->delfun = f;
	l->delarg = arg;
	l->state = learner_new();
	
	// setup connections to acceptors
	l->acceptors = peers_new(b, c->acceptors_count);
	for (i = 0; i < c->acceptors_count; i++)
		peers_connect(l->acceptors, &c->acceptors[i], on_acceptor_msg, l);
	
	// setup hole checking timer
	l->tv.tv_sec = 0;
	l->tv.tv_usec = 100000;
	l->hole_timer = evtimer_new(b, learner_check_holes, l);
	event_add(l->hole_timer, &l->tv);
	
	free_config(c);
	
	LOG(VRB, ("Learner is ready\n"));
	return l;
}

struct evlearner*
evlearner_init(const char* config_file, deliver_function f, void* arg, 
	struct event_base* b)
{
	struct config* c = read_config(config_file);
	if (c == NULL) return NULL;
	return evlearner_init_conf(c, f, arg, b);
}

void
evlearner_free(struct evlearner* l)
{
	peers_free(l->acceptors);
	event_free(l->hole_timer);
	learner_free(l->state);
	free(l);
}

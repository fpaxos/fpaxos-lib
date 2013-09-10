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
#include "learner.h"
#include "peers.h"
#include "tcp_sendbuf.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

struct evlearner
{
	struct learner* state;      /* The actual learner */
	deliver_function delfun;    /* Delivery callback */
	void* delarg;               /* The argument to the delivery callback */
	struct event* hole_timer;   /* Timer to check for holes */
	struct timeval tv;          /* Check for holes every tv units of time */
	struct peers* acceptors;    /* Connections to acceptors */
};


static void
evlearner_check_holes(evutil_socket_t fd, short event, void *arg)
{
	paxos_repeat msg;
	int i, chunks = 10000;
	struct evlearner* l = arg;
	if (learner_has_holes(l->state, &msg.from, &msg.to)) {
		if ((msg.to - msg.from) > chunks)
			msg.to = msg.from + chunks;
		for (i = 0; i < peers_count(l->acceptors); i++) {
			struct bufferevent* bev = peers_get_buffer(l->acceptors, i);
			send_paxos_repeat(bev, &msg);
		}
	}
	event_add(l->hole_timer, &l->tv);
}

static void 
evlearner_deliver_next_closed(struct evlearner* l)
{
	int prop_id;
	paxos_accepted deliver;
	while (learner_deliver_next(l->state, &deliver)) {
		prop_id = deliver.ballot % MAX_N_OF_PROPOSERS;
		l->delfun(deliver.value.value_val, deliver.value.value_len,
			deliver.iid, deliver.ballot, prop_id, l->delarg);
	}
}

/*
	Called when an accept_ack is received, the learner will update it's status
    for that instance and afterwards check if the instance is closed
*/
static void
evlearner_handle_accepted(struct evlearner* l, paxos_accepted* msg, int from)
{
	learner_receive_accepted(l->state, msg, from);
	evlearner_deliver_next_closed(l);
}

static void
evlearner_handle_msg(paxos_message* msg, int from, void* arg)
{
	struct evlearner* l = arg;
	switch (msg->type) {
		case PAXOS_ACCEPTED:
			evlearner_handle_accepted(l, &msg->paxos_message_u.accepted, from);
			break;
		default:
			paxos_log_error("Unknow msg type %d not handled", msg->type);
    }
}

static struct evlearner*
evlearner_init_conf(struct evpaxos_config* c, deliver_function f, void* arg, 
	struct event_base* b)
{
	struct evlearner* l;
	int acceptor_count = evpaxos_acceptor_count(c);
	
	l = malloc(sizeof(struct evlearner));
	l->delfun = f;
	l->delarg = arg;
	l->state = learner_new(acceptor_count);
	
	// setup connections to acceptors
	l->acceptors = peers_new(b, c);
	peers_connect_to_acceptors(l->acceptors, evlearner_handle_msg, l);
	
	// setup hole checking timer
	l->tv.tv_sec = 0;
	l->tv.tv_usec = 100000;
	l->hole_timer = evtimer_new(b, evlearner_check_holes, l);
	event_add(l->hole_timer, &l->tv);
	
	evpaxos_config_free(c);
	return l;
}

struct evlearner*
evlearner_init(const char* config_file, deliver_function f, void* arg, 
	struct event_base* b)
{
	struct evpaxos_config* c = evpaxos_config_read(config_file);
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

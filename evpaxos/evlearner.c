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
#include "learner.h"
#include "peers.h"
#include "message.h"
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
	paxos_accepted* deliver;
	while ((deliver = learner_deliver_next(l->state)) != NULL) {
		prop_id = deliver->ballot % MAX_N_OF_PROPOSERS;
		l->delfun(
			deliver->value.paxos_value_val,
			deliver->value.paxos_value_len,
			deliver->iid, deliver->ballot, prop_id, l->delarg);
		paxos_accepted_free(deliver);
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
			evlearner_handle_accepted(l, &msg->u.accepted, from);
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

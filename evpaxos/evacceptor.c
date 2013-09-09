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


#include <event2/event.h>
#include <event2/util.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>

#include "evpaxos.h"
#include "config.h"
#include "tcp_receiver.h"
#include "acceptor.h"
#include "tcp_sendbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


struct evacceptor
{
	int acceptor_id;
	struct acceptor* state;
	struct event_base* base;
	struct tcp_receiver* receiver;
	struct evpaxos_config* conf;
};


/*
	Received a prepare request (phase 1a).
*/
static void 
evacceptor_handle_prepare(struct evacceptor* a, 
	struct bufferevent* bev, paxos_prepare* m)
{
	paxos_log_debug("Handle prepare for iid %d ballot %d", m->iid, m->ballot);
	paxos_promise promise;
	acceptor_receive_prepare(a->state, m, &promise);
	send_paxos_promise(bev, &promise);
}

/*
	Received a accept request (phase 2a).
*/
static void 
evacceptor_handle_accept(struct evacceptor* a,
	struct bufferevent* bev, paxos_accept* accept)
{	
	int i;
	paxos_accepted accepted;
	struct carray* bevs = tcp_receiver_get_events(a->receiver);
	paxos_log_debug("Handle accept for iid %d bal %d", 
		accept->iid, accept->ballot);
	acceptor_receive_accept(a->state, accept, &accepted);
	if (accept->ballot == accepted.ballot) // accepted!
		for (i = 0; i < carray_count(bevs); i++)
			send_paxos_accepted(carray_at(bevs, i), &accepted);
	else
		send_paxos_accepted(bev, &accepted); // send nack
}

static void
evacceptor_handle_repeat(struct evacceptor* a,
	struct bufferevent* bev, paxos_repeat* repeat)
{	
	iid_t iid;
	paxos_accepted accepted;
	paxos_log_debug("Handle repeat for iids %d-%d", repeat->from, repeat->to);
	for (iid = repeat->from; iid <= repeat->to; ++iid) {
		if (acceptor_receive_repeat(a->state, iid, &accepted))
			send_paxos_accepted(bev, &accepted);
	}
}

/*
	This function is invoked when a new paxos message has been received.
*/
static void 
evacceptor_handle_msg(struct bufferevent* bev, paxos_message* msg, void* arg)
{
	struct evacceptor* a = (struct evacceptor*)arg;
	switch (msg->type) {
		case PAXOS_PREPARE:
			evacceptor_handle_prepare(a, bev, &msg->paxos_message_u.prepare);
			break;
		case PAXOS_ACCEPT:
			evacceptor_handle_accept(a, bev, &msg->paxos_message_u.accept);
			break;
		case PAXOS_REPEAT:
			evacceptor_handle_repeat(a, bev, &msg->paxos_message_u.repeat);
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
	a->receiver = tcp_receiver_new(a->base, port, evacceptor_handle_msg, a);
	a->state = acceptor_new(id);

    return a;
}

int
evacceptor_free(struct evacceptor* a)
{
	acceptor_free(a->state);
	tcp_receiver_free(a->receiver);
	evpaxos_config_free(a->conf);
	free(a);
	return 0;
}

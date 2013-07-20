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
#include "libpaxos_messages.h"
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
handle_prepare_req(struct evacceptor* a, 
	struct bufferevent* bev, prepare_req* pr)
{
	paxos_log_debug("Handling prepare for instance %d ballot %d",
		pr->iid, pr->ballot);
	
	acceptor_record * rec;
	rec = acceptor_receive_prepare(a->state, pr);
	sendbuf_add_prepare_ack(bev, rec);
}

/*
	Received a accept request (phase 2a).
*/
static void 
handle_accept_req(struct evacceptor* a,
	struct bufferevent* bev, accept_req* ar)
{
	paxos_log_debug("Handling accept for instance %d ballot %d", 
		ar->iid, ar->ballot);

	int i;
	struct carray* bevs = tcp_receiver_get_events(a->receiver);
	acceptor_record* rec = acceptor_receive_accept(a->state, ar);
	if (ar->ballot == rec->ballot) // accepted!
		for (i = 0; i < carray_count(bevs); i++)
			sendbuf_add_accept_ack(carray_at(bevs, i), rec);
	else
		sendbuf_add_accept_ack(bev, rec); // send nack
}

static void
handle_repeat_req(struct evacceptor* a, struct bufferevent* bev, iid_t iid)
{
	paxos_log_debug("Handling repeat for instance %d", iid);
	acceptor_record* rec = acceptor_receive_repeat(a->state, iid);
	if (rec != NULL)
		sendbuf_add_accept_ack(bev, rec);
}

/*
	This function is invoked when a new message is ready to be read.
*/
static void 
handle_req(struct bufferevent* bev, void* arg)
{
	paxos_msg msg;
	struct evbuffer* in;
	char buffer[PAXOS_MAX_VALUE_SIZE];
	struct evacceptor* a = (struct evacceptor*)arg;
	
	in = bufferevent_get_input(bev);
	evbuffer_remove(in, &msg, sizeof(paxos_msg));
	if (msg.data_size > PAXOS_MAX_VALUE_SIZE) {
		evbuffer_drain(in, msg.data_size);
		paxos_log_error("Discarding message of size %ld. Maximum is %d",
			msg.data_size, PAXOS_MAX_VALUE_SIZE);
		return;
	}
	evbuffer_remove(in, buffer, msg.data_size);
	
	switch (msg.type) {
		case prepare_reqs:
			handle_prepare_req(a, bev, (prepare_req*)buffer);
			break;
		case accept_reqs:
			handle_accept_req(a, bev, (accept_req*)buffer);
			break;
		case repeat_reqs:
			handle_repeat_req(a, bev, *((iid_t*)buffer));
			break;
		default:
			paxos_log_error("Unknow msg type %d not handled", msg.type);
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
	a->receiver = tcp_receiver_new(a->base, port, handle_req, a);
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

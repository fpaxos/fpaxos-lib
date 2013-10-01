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

#include "peers.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <event2/bufferevent.h>

struct peer
{
	struct bufferevent* bev;
	struct event* reconnect_ev;
	struct sockaddr_in addr;
	bufferevent_data_cb cb;
	void* arg;
};

struct peers
{
	int count;
	struct peer** peers;
	struct event_base* base;
};

static struct timeval reconnect_timeout = {2,0};
static struct peer* make_peer(struct event_base* base, 
	struct sockaddr_in* addr, bufferevent_data_cb cb, void* arg);
static void free_peer(struct peer* p);
static void connect_peer(struct peer* p);


struct peers*
peers_new(struct event_base* base)
{
	struct peers* p = malloc(sizeof(struct peers));
	p->count = 0;
	p->peers = NULL;
	p->base = base;
	return p;
}

void
peers_free(struct peers* p)
{
	int i;
	for (i = 0; i < p->count; i++)
		free_peer(p->peers[i]);		
	if (p->count > 0)
		free(p->peers);
	free(p);
}

void
peers_connect(struct peers* p, struct sockaddr_in* addr,
	bufferevent_data_cb cb, void* arg)
{
	p->peers = realloc(p->peers, sizeof(struct peer*) * (p->count+1));
	p->peers[p->count] = make_peer(p->base, addr, cb, arg);
	p->count++;
}

void
peers_connect_to_acceptors(struct peers* p, struct evpaxos_config* c,
	bufferevent_data_cb cb, void* arg)
{
	int i;
	for (i = 0; i < evpaxos_acceptor_count(c); i++) {
		struct sockaddr_in addr = evpaxos_acceptor_address(c, i);
		peers_connect(p, &addr, cb, arg);
	}
}

int
peers_count(struct peers* p)
{
	return p->count;
}

struct bufferevent*
peers_get_buffer(struct peers* p, int i)
{
	return p->peers[i]->bev;
}

static void
on_read(struct bufferevent* bev, void* arg) 
{
	struct peer* p = arg;
	p->cb(bev, p->arg);
}

static void
on_socket_event(struct bufferevent* bev, short ev, void *arg)
{
	struct peer* p = (struct peer*)arg;
	
	if (ev & BEV_EVENT_CONNECTED) {
		paxos_log_info("Connected to %s:%d",
			inet_ntoa(p->addr.sin_addr), ntohs(p->addr.sin_port));
	} else if (ev & BEV_EVENT_ERROR || ev & BEV_EVENT_EOF) {
		struct event_base* base;
		int err = EVUTIL_SOCKET_ERROR();
		paxos_log_error("%s (%s:%d)", evutil_socket_error_to_string(err),
			inet_ntoa(p->addr.sin_addr), ntohs(p->addr.sin_port));
		base = bufferevent_get_base(p->bev);
		bufferevent_free(p->bev);
		p->bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(p->bev, on_read, NULL, on_socket_event, p);
		event_add(p->reconnect_ev, &reconnect_timeout);
	} else {
		paxos_log_error("Event %d not handled", ev);
	}
}

static void
on_connection_timeout(int fd, short ev, void* arg)
{
	connect_peer((struct peer*)arg);
}

static void
connect_peer(struct peer* p)
{
	bufferevent_enable(p->bev, EV_READ|EV_WRITE);
	bufferevent_socket_connect(p->bev, 
		(struct sockaddr*)&p->addr, sizeof(p->addr));
	paxos_log_info("Connect to %s:%d", 
		inet_ntoa(p->addr.sin_addr), ntohs(p->addr.sin_port));
}

static struct peer*
make_peer(struct event_base* base, struct sockaddr_in* addr,
	bufferevent_data_cb cb, void* arg)
{
	struct peer* p = malloc(sizeof(struct peer));
	p->addr = *addr;
	p->reconnect_ev = evtimer_new(base, on_connection_timeout, p);
	p->bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(p->bev, on_read, NULL, on_socket_event, p);
	p->cb = cb;
	p->arg = arg;
	connect_peer(p);	
	return p;
}

static void
free_peer(struct peer* p)
{
	bufferevent_free(p->bev);
	event_free(p->reconnect_ev);
	free(p);
}

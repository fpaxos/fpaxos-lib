/*
 * Copyright (c) 2013-2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "peers.h"
#include "message.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <netinet/tcp.h>


struct peer
{
	int id;
	int status;
	struct bufferevent* bev;
	struct event* reconnect_ev;
	struct sockaddr_in addr;
	struct peers* peers;
};

struct subscription
{
	paxos_message_type type;
	peer_cb callback;
	void* arg;
};

struct peers
{
	int peers_count, clients_count;
	struct peer** peers;   /* peers we connected to */
	struct peer** clients; /* peers we accepted connections from */
	struct evconnlistener* listener;
	struct event_base* base;
	struct evpaxos_config* config;
	int subs_count;
	struct subscription subs[32];
};

static struct timeval reconnect_timeout = {2,0};
static struct peer* make_peer(struct peers* p, int id, struct sockaddr_in* in);
static void free_peer(struct peer* p);
static void free_all_peers(struct peer** p, int count);
static void connect_peer(struct peer* p);
static void peers_connect(struct peers* p, int id, struct sockaddr_in* addr);
static void on_read(struct bufferevent* bev, void* arg);
static void on_peer_event(struct bufferevent* bev, short ev, void *arg);
static void on_client_event(struct bufferevent* bev, short events, void *arg);
static void on_connection_timeout(int fd, short ev, void* arg);
static void on_listener_error(struct evconnlistener* l, void* arg);
static void on_accept(struct evconnlistener *l, evutil_socket_t fd,
	struct sockaddr* addr, int socklen, void *arg);
static void socket_set_nodelay(int fd);

struct peers*
peers_new(struct event_base* base, struct evpaxos_config* config)
{
	struct peers* p = malloc(sizeof(struct peers));
	p->peers_count = 0;
	p->clients_count = 0;
	p->subs_count = 0;
	p->peers = NULL;
	p->clients = NULL;
	p->listener = NULL;
	p->base = base;
	p->config = config;
	return p;
}

void
peers_free(struct peers* p)
{
	free_all_peers(p->peers, p->peers_count);
	free_all_peers(p->clients, p->clients_count);
	if (p->listener != NULL)
		evconnlistener_free(p->listener);
	free(p);
}

int
peers_count(struct peers* p)
{
	return p->peers_count;
}

static void
peers_connect(struct peers* p, int id, struct sockaddr_in* addr)
{
	p->peers = realloc(p->peers, sizeof(struct peer*) * (p->peers_count+1));
	p->peers[p->peers_count] = make_peer(p, id, addr);
	
	struct peer* peer = p->peers[p->peers_count];
	bufferevent_setcb(peer->bev, on_read, NULL, on_peer_event, peer);
	peer->reconnect_ev = evtimer_new(p->base, on_connection_timeout, peer);
	connect_peer(peer);

	p->peers_count++;
}

void
peers_connect_to_acceptors(struct peers* p)
{
	int i;
	for (i = 0; i < evpaxos_acceptor_count(p->config); i++) {
		struct sockaddr_in addr = evpaxos_acceptor_address(p->config, i);
		peers_connect(p, i, &addr);
	}
}

void
peers_foreach_acceptor(struct peers* p, peer_iter_cb cb, void* arg)
{
	int i;
	for (i = 0; i < p->peers_count; ++i)
		cb(p->peers[i], arg);
}

void
peers_foreach_client(struct peers* p, peer_iter_cb cb, void* arg)
{
	int i;
	for (i = 0; i < p->clients_count; ++i)
		cb(p->clients[i], arg);
}

struct peer*
peers_get_acceptor(struct peers* p, int id)
{
	int i;
	for (i = 0; p->peers_count; ++i)
		if (p->peers[i]->id == id)
			return p->peers[i];
	return NULL;
}

struct bufferevent*
peer_get_buffer(struct peer* p)
{
	return p->bev;
}

int
peer_get_id(struct peer* p)
{
	return p->id;
}

int peer_connected(struct peer* p)
{
	return p->status == BEV_EVENT_CONNECTED;
}

int
peers_listen(struct peers* p, int port)
{
	struct sockaddr_in addr;
	unsigned flags = LEV_OPT_CLOSE_ON_EXEC
		| LEV_OPT_CLOSE_ON_FREE
		| LEV_OPT_REUSEABLE;
	
	/* listen on the given port at address 0.0.0.0 */
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0);
	addr.sin_port = htons(port);
	
	p->listener = evconnlistener_new_bind(p->base, on_accept, p, flags, -1,
		(struct sockaddr*)&addr, sizeof(addr));
	if (p->listener == NULL) {
		paxos_log_error("Failed to bind on port %d", port);
		return 0;
	}
	evconnlistener_set_error_cb(p->listener, on_listener_error);
	paxos_log_info("Listening on port %d", port);
	return 1;
}

void
peers_subscribe(struct peers* p, paxos_message_type type, peer_cb cb, void* arg)
{
	struct subscription* sub = &p->subs[p->subs_count];
	sub->type = type;
	sub->callback = cb;
	sub->arg = arg;
	p->subs_count++;
}

struct event_base*
peers_get_event_base(struct peers* p)
{
	return p->base;
}

static void
dispatch_message(struct peer* p, paxos_message* msg)
{
	int i;
	for (i = 0; i < p->peers->subs_count; ++i) {
		struct subscription* sub = &p->peers->subs[i];
		if (sub->type == msg->type)
			sub->callback(p, msg, sub->arg);
	}
}

static void
on_read(struct bufferevent* bev, void* arg)
{
	paxos_message msg;
	struct peer* p = (struct peer*)arg;
	struct evbuffer* in = bufferevent_get_input(bev);
	while (recv_paxos_message(in, &msg)) {
		dispatch_message(p, &msg);
		paxos_message_destroy(&msg);
	}
}

static void
on_peer_event(struct bufferevent* bev, short ev, void *arg)
{
	struct peer* p = (struct peer*)arg;
	
	if (ev & BEV_EVENT_CONNECTED) {
		paxos_log_info("Connected to %s:%d",
			inet_ntoa(p->addr.sin_addr), ntohs(p->addr.sin_port));
		p->status = ev;
	} else if (ev & BEV_EVENT_ERROR || ev & BEV_EVENT_EOF) {
		struct event_base* base;
		int err = EVUTIL_SOCKET_ERROR();
		paxos_log_error("%s (%s:%d)", evutil_socket_error_to_string(err),
			inet_ntoa(p->addr.sin_addr), ntohs(p->addr.sin_port));
		base = bufferevent_get_base(p->bev);
		bufferevent_free(p->bev);
		p->bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
		bufferevent_setcb(p->bev, on_read, NULL, on_peer_event, p);
		event_add(p->reconnect_ev, &reconnect_timeout);
		p->status = ev;
	} else {
		paxos_log_error("Event %d not handled", ev);
	}
}

static void
on_client_event(struct bufferevent* bev, short ev, void *arg)
{
	struct peer* p = (struct peer*)arg;
	if (ev & BEV_EVENT_EOF || ev & BEV_EVENT_ERROR) {
		int i;
		struct peer** clients = p->peers->clients;
		for (i = p->id; i < p->peers->clients_count-1; ++i) {
			clients[i] = clients[i+1];
			clients[i]->id = i;
		}
		p->peers->clients_count--;
		p->peers->clients = realloc(p->peers->clients, 
			sizeof(struct peer*) * (p->peers->clients_count));
		free_peer(p);
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
on_listener_error(struct evconnlistener* l, void* arg)
{
	int err = EVUTIL_SOCKET_ERROR();
	struct event_base *base = evconnlistener_get_base(l);
	paxos_log_error("Listener error %d: %s. Shutting down event loop.", err,
		evutil_socket_error_to_string(err));
	event_base_loopexit(base, NULL);
}

static void
on_accept(struct evconnlistener *l, evutil_socket_t fd,
	struct sockaddr* addr, int socklen, void *arg)
{
	struct peer* peer;
	struct peers* peers = arg;

	peers->clients = realloc(peers->clients,
		sizeof(struct peer*) * (peers->clients_count+1));
	peers->clients[peers->clients_count] = 
		make_peer(peers, peers->clients_count, (struct sockaddr_in*)addr);
	
	peer = peers->clients[peers->clients_count];
	bufferevent_setfd(peer->bev, fd);
	bufferevent_setcb(peer->bev, on_read, NULL, on_client_event, peer);
	bufferevent_enable(peer->bev, EV_READ|EV_WRITE);
	socket_set_nodelay(fd);
	
	paxos_log_info("Accepted connection from %s:%d",
		inet_ntoa(((struct sockaddr_in*)addr)->sin_addr),
		ntohs(((struct sockaddr_in*)addr)->sin_port));
	
	peers->clients_count++;
}

static void
connect_peer(struct peer* p)
{
	bufferevent_enable(p->bev, EV_READ|EV_WRITE);
	bufferevent_socket_connect(p->bev, 
		(struct sockaddr*)&p->addr, sizeof(p->addr));
	socket_set_nodelay(bufferevent_getfd(p->bev));
	paxos_log_info("Connect to %s:%d", 
		inet_ntoa(p->addr.sin_addr), ntohs(p->addr.sin_port));
}

static struct peer*
make_peer(struct peers* peers, int id, struct sockaddr_in* addr)
{
	struct peer* p = malloc(sizeof(struct peer));
	p->id = id;
	p->addr = *addr;
	p->bev = bufferevent_socket_new(peers->base, -1, BEV_OPT_CLOSE_ON_FREE);
	p->peers = peers;
	p->reconnect_ev = NULL;
	p->status = BEV_EVENT_EOF;
	return p;
}

static void
free_all_peers(struct peer** p, int count)
{
	int i;
	for (i = 0; i < count; i++)
		free_peer(p[i]);
	if (count > 0)
		free(p);
}

static void
free_peer(struct peer* p)
{
	bufferevent_free(p->bev);
	if (p->reconnect_ev != NULL)
		event_free(p->reconnect_ev);
	free(p);
}

static void
socket_set_nodelay(int fd)
{
	int flag = paxos_config.tcp_nodelay;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
}

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


#include "tcp_receiver.h"
#include "libpaxos_messages.h"

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <event2/listener.h>
#include <event2/buffer.h>


static void 
set_sockaddr_in(struct sockaddr_in* sin, struct address* a)
{
	memset(sin, 0, sizeof(sin));
	sin->sin_family = AF_INET;
	/* Listen on 0.0.0.0 */
	sin->sin_addr.s_addr = htonl(0);
	/* Listen on the given port. */
	sin->sin_port = htons(a->port);
}

static void
on_read(struct bufferevent* bev, void* arg)
{
	size_t len;
	paxos_msg msg;
	struct evbuffer* in;
	struct tcp_receiver* r = arg;
	
	in = bufferevent_get_input(bev);
	
	while ((len = evbuffer_get_length(in)) > sizeof(paxos_msg)) {
		evbuffer_copyout(in, &msg, sizeof(paxos_msg));
		if (len < PAXOS_MSG_SIZE((&msg)))
			return;
		r->callback(bev, r->arg);
	}
}

static int
match_bufferevent(void* arg, void* item)
{
	return arg == item;
}

static void
on_error(struct bufferevent *bev, short events, void *arg)
{
	struct tcp_receiver* r = arg;
	if (events & (BEV_EVENT_EOF)) {
		struct carray* tmp = carray_reject(r->bevs, match_bufferevent, bev);
		carray_free(r->bevs);
		r->bevs = tmp;
		bufferevent_free(bev);
	}
}

static void
on_accept(struct evconnlistener *l, evutil_socket_t fd,
	struct sockaddr* addr, int socklen, void *arg)
{
	struct tcp_receiver* r = arg;
	struct event_base* b = evconnlistener_get_base(l);
	struct bufferevent *bev = bufferevent_socket_new(b, fd, 
		BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, on_read, NULL, on_error, arg);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	carray_push_back(r->bevs, bev);
	paxos_log_info("Accepted connection from %s:%d",
		inet_ntoa(((struct sockaddr_in*)addr)->sin_addr),
		ntohs(((struct sockaddr_in*)addr)->sin_port));
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

struct tcp_receiver*
tcp_receiver_new(struct event_base* b, struct address* a,
 	bufferevent_data_cb cb, void* arg)
{
	struct tcp_receiver* r;
	struct sockaddr_in sin;
	unsigned flags = LEV_OPT_CLOSE_ON_EXEC
		| LEV_OPT_CLOSE_ON_FREE
		| LEV_OPT_REUSEABLE;
	
	r = malloc(sizeof(struct tcp_receiver));
	set_sockaddr_in(&sin, a);
	r->callback = cb;
	r->arg = arg;
	r->listener = evconnlistener_new_bind(
		b, on_accept, r, flags,	-1, (struct sockaddr*)&sin, sizeof(sin));
	assert(r->listener != NULL);
	evconnlistener_set_error_cb(r->listener, on_listener_error);
	r->bevs = carray_new(10);
	paxos_log_info("Listening on port %d", a->port);
	
	return r;
}

void 
tcp_receiver_free(struct tcp_receiver* r)
{
	int i;
	for (i = 0; i < carray_count(r->bevs); ++i)
		bufferevent_free(carray_at(r->bevs, i));
	evconnlistener_free(r->listener);
	carray_free(r->bevs);
	free(r);
}

struct carray*
tcp_receiver_get_events(struct tcp_receiver* r)
{
	return r->bevs;
}

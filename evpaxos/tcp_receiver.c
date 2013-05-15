#include "tcp_receiver.h"

#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <event2/listener.h>
#include <event2/buffer.h>


static void 
set_sockaddr_in(struct sockaddr_in* sin, address* a)
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
		if (len < PAXOS_MSG_SIZE((&msg))) {
			LOG(DBG, ("not enough data\n"));
			return;
		}
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
		r->bevs = carray_reject(r->bevs, match_bufferevent, bev);
		bufferevent_free(bev);
	}
}

static void
on_accept(struct evconnlistener *l, evutil_socket_t fd,
	struct sockaddr *addr, int socklen, void *arg)
{
	struct tcp_receiver* r = arg;
	struct event_base* b = evconnlistener_get_base(l);
	struct bufferevent *bev = bufferevent_socket_new(b, fd, 
		BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, on_read, NULL, on_error, arg);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
	carray_push_back(r->bevs, bev);
	LOG(VRB, ("accepted connection from...\n"));
}

static void
on_listener_error(struct evconnlistener* l, void* arg)
{
	struct event_base *base = evconnlistener_get_base(l);
	int err = EVUTIL_SOCKET_ERROR();
	fprintf(stderr, "Got an error %d (%s) on the listener. "
		"Shutting down.\n", err, evutil_socket_error_to_string(err));

	event_base_loopexit(base, NULL);
}

struct tcp_receiver*
tcp_receiver_new(struct event_base* b, address* a,
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
	
	return r;
}

void 
tcp_receiver_free(struct tcp_receiver* r)
{
	evconnlistener_free(r->listener);
	carray_free(r->bevs);
	free(r);
}

struct carray*
tcp_receiver_get_events(struct tcp_receiver* r)
{
	return r->bevs;
}

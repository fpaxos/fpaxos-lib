/*
 * Copyright (c) 2015, University of Lugano
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


#include <evpaxos.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <event2/event.h>
#include "test_client.h"


static void
on_connect(struct bufferevent* bev, short events, void* arg)
{
	if (events & BEV_EVENT_ERROR)
		printf("%s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
}

static struct bufferevent* 
connect_to_proposer(struct test_client* c, const char* config, int proposer_id)
{
	struct bufferevent* bev;
	struct evpaxos_config* conf = evpaxos_config_read(config);
	struct sockaddr_in addr = evpaxos_proposer_address(conf, proposer_id);
	bev = bufferevent_socket_new(c->base, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, NULL, NULL, on_connect, c);
	bufferevent_enable(bev, EV_WRITE);
	bufferevent_socket_connect(bev, (struct sockaddr*)&addr, sizeof(addr));
	event_base_dispatch(c->base);
	return bev;
}

void
test_client_submit_value(struct test_client* c, int value)
{
	paxos_submit(c->bev, (char*)&value, sizeof(int));
	event_base_dispatch(c->base);
}

struct test_client*
test_client_new(const char* config, int proposer_id)
{
	struct test_client* c;
	c = malloc(sizeof(struct test_client));
	c->base = event_base_new();
	c->bev = connect_to_proposer(c, config, proposer_id);
	if (c->bev == NULL)
		return NULL;
	return c;
}

void
test_client_free(struct test_client* c)
{
	bufferevent_free(c->bev);
	event_base_free(c->base);
	free(c);
}

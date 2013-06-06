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


#ifndef _TCP_RECEIVER_H_
#define _TCP_RECEIVER_H_

#include "evpaxos.h"
#include "carray.h"
#include <event2/event.h>
#include <event2/bufferevent.h>

struct tcp_receiver
{
	bufferevent_data_cb callback;
	void* arg;
	struct evconnlistener* listener;
	struct carray* bevs;
};

struct tcp_receiver*
tcp_receiver_new(struct event_base* b, address* a, bufferevent_data_cb callback, void* arg);

void 
tcp_receiver_free(struct tcp_receiver* r);

struct carray* 
tcp_receiver_get_events(struct tcp_receiver* r);

#endif

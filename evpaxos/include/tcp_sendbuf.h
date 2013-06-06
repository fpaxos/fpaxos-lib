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


#ifndef _TCP_SENDBUF_H_
#define _TCP_SENDBUF_H_

#include "evpaxos.h"
#include "libpaxos_messages.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <event2/bufferevent.h>

void sendbuf_add_prepare_ack(struct bufferevent* bev, acceptor_record* rec);
void sendbuf_add_prepare_req(struct bufferevent* bev, prepare_req* pr);
void sendbuf_add_accept_req(struct bufferevent* bev, accept_req* ar);
void sendbuf_add_accept_ack(struct bufferevent* bev, acceptor_record* rec);
void sendbuf_add_repeat_req(struct bufferevent* bev, iid_t iid);

#endif

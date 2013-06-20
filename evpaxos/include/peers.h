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


#ifndef _PEERS_H_
#define _PEERS_H_

#include "config_reader.h"
#include <event2/bufferevent.h>

struct peers;

struct peers* peers_new(struct event_base* base, int count);
void peers_free(struct peers* p);
void peers_connect(struct peers* p, struct address* a, bufferevent_data_cb cb, void* arg);
int peers_count(struct peers* p);
struct bufferevent* peers_get_buffer(struct peers* p, int i);

#endif

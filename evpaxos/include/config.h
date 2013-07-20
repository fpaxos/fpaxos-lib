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


#ifndef _CONFIG_READER_H_
#define _CONFIG_READER_H_

#include "evpaxos.h"

struct evpaxos_config;

struct evpaxos_config* evpaxos_config_read(const char* path);
void evpaxos_config_free(struct evpaxos_config* config);

int paxos_proposer_count(struct evpaxos_config* c);
struct sockaddr_in evpaxos_proposer_address(struct evpaxos_config* c, int i);
int evpaxos_proposer_listen_port(struct evpaxos_config* c, int i);
int evpaxos_acceptor_count(struct evpaxos_config* config);
struct sockaddr_in evpaxos_acceptor_address(struct evpaxos_config* c, int i);
int evpaxos_acceptor_listen_port(struct evpaxos_config* c, int i);


#endif

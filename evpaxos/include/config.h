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

// TODO remove MAX_ADDR
#define MAX_ADDR 10

struct address
{
	char* addr;
	int port;
};

struct evpaxos_config
{
	int proposers_count;
	int acceptors_count;
	struct address proposers[MAX_ADDR];
	struct address acceptors[MAX_ADDR];
};

struct evpaxos_config* evpaxos_config_read(const char* path);
void evpaxos_config_free(struct evpaxos_config* config);

#endif

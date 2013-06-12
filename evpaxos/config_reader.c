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


#include "config_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int fields = 4;

static void
print_config(address* a, int count)
{
	int i;
	for (i = 0; i < count; i++)
		printf("%s %d\n", a[i].address_string, a[i].port);
}

static address*
address_init(address* a, char* addr, int port)
{
	a->address_string = strdup(addr);
	a->port = port;
	return a;
}

static void
address_free(address* a)
{
	free(a->address_string);
}

struct config*
read_config(const char* path)
{
	int id;
	char type;
	address a;
	address* tmp;
	struct config* c;
	FILE* f;

	f = fopen(path, "r");
	if (f == NULL) {
		perror("fopen"); return NULL;
	}
	
	c = malloc(sizeof(struct config));
	memset(c, 0, sizeof(struct config));
	a.address_string = malloc(128);
	
	while(fscanf(f, "%c %d %s %d\n", &type, &id,
		a.address_string, &a.port) == fields) {
			
		switch(type) {
			case 'p':
				tmp = &c->proposers[c->proposers_count++];
				address_init(tmp, a.address_string, a.port);
				break;
			case 'a':
				tmp = &c->acceptors[c->acceptors_count++];
				address_init(tmp, a.address_string, a.port);
				break;
		}
	}
	
	printf("proposers\n");
	print_config(c->proposers, c->proposers_count);
	printf("acceptors\n");
	print_config(c->acceptors , c->acceptors_count);
	
	fclose(f);
	free(a.address_string);
	
	return c;
}

void
free_config(struct config* c)
{
	int i;
	for (i = 0; i < c->proposers_count; ++i)
		address_free(&c->proposers[i]);
	for (i = 0; i < c->acceptors_count; ++i)
		address_free(&c->acceptors[i]);
	free(c);
}



// enum option_type 
// {
// 	option_boolean,
// 	option_integer,
// 	option_string,
// };
// 
// struct option
// {
// 	const char* name;
// 	void* value;
// 	enum option_type type;
// };

// struct paxos_config
// { 
// 	/* Learner */
// 	int learner_instances;
// 	
// 	/* Proposer */
// 	
// 	/* Acceptor */
// 	
// 	/* BDB storage configuration */
// 	int bdb_sync;
// 	int bdb_transactional;
// 	int bdb_cachesize;
// 	char* bdb_env_path;
// 	char* bdb_db_filename;
// };
// 
// struct paxos_config config =
// {
// 	0,             /* bdb_sync */
// 	0,             /* bdb_transactional */
// 	32*1024*1023,  /* bdb_cachesize */
// 	"/tmp",		   /* bdb_env_path */
// 	"acc.bdb",     /* bdb_db_filename */
// };

// struct option options[] = {
// 	{ "bdb-sync", &config.bdb_sync, option_boolean },
// 	{ "bdb-transactional", &config.transactional, option_boolean },
// 	{ "bdb-env-path", &config.bdb_env_path, option_string },
// 	{ "bdb-db-name", &config.bdb_db_name, option_string }
// }


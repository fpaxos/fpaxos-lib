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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


enum option_type
{
	option_boolean,
	option_integer,
	option_string
};

struct option
{
	const char* name;
	void* value;
	enum option_type type;
};

struct option options[] =
{
	{ "learner-catch-up", &paxos_config.learner_catch_up, option_boolean },
	{ "proposer_preexec_window", &paxos_config.proposer_preexec_window, option_integer },
	{ "bdb-sync", &paxos_config.bdb_sync, option_boolean },
	{ "bdb-cachesize", &paxos_config.bdb_cachesize, option_integer },
	{ "bdb-env-path", &paxos_config.bdb_env_path, option_string },
	{ "bdb-db-filename", &paxos_config.bdb_db_filename, option_string },
	{ "bdb-trash-files", &paxos_config.bdb_trash_files, option_boolean },
	{ 0 }
};


static void
address_init(struct address* a, char* addr, int port)
{
	a->address_string = strdup(addr);
	a->port = port;
}

static void
address_free(struct address* a)
{
	free(a->address_string);
}

static char*
strtrim(char* string)
{
	char *s, *t;
	for (s = string; isspace(*s); s++)
		;
	if (*s == 0)
		return s;
	t = s + strlen(s) - 1;
	while (t > s && isspace(*t))
		t--;
	*++t = '\0';
	return s;
}

static int
parse_boolean(char* str, int* boolean)
{
	if (str == NULL) return 0;
    if (strcasecmp(str, "yes") == 0) {
    	*boolean = 1;
		return 1;
	}
	if (strcasecmp(str, "no") == 0) {
		*boolean = 0;
		return 1;
	}	
	return 0;
}

static int
parse_integer(char* str, int* integer)
{
	int n;
	char* end;
	if (str == NULL) return 0;
	n = strtol(str, &end, 10);
	if (end == str) return 0;
	*integer = n;
	return 1;
}

static int
parse_string(char* str, char** string)
{
	if (str == NULL || str[0] == '\0' || str[0] == '\n')
		return 0;
	*string = strdup(str);
	return 1;
}

static int
parse_address(char* str, struct address* addr)
{
	int id;
	int port;
	char address[128];
	int rv = sscanf(str, "%d %s %d", &id, address, &port);
	if (rv == 3) {
		address_init(addr, address, port);
		return 1;
	}
	return 0;
}

static struct option*
lookup_option(char* opt)
{
	int i = 0;
	while (options[i].name != NULL) {
		if (strcasecmp(options[i].name, opt) == 0)
			return &options[i];
		i++;
	}
	return NULL;
}

static int 
parse_line(char* line, struct config* c)
{
	int rv;
	char* tok;
	char* sep = " ";
	struct option* opt;
	
	line = strtrim(line);
	tok = strsep(&line, sep);
	
	if (strcasecmp(tok, "a") == 0) {
		struct address* addr = &c->acceptors[c->acceptors_count++];
		return parse_address(line, addr);
	}
	
	if (strcasecmp(tok, "p") == 0) {
		struct address* addr = &c->proposers[c->proposers_count++];
		return parse_address(line, addr);
	}
	
	line = strtrim(line);
	opt = lookup_option(tok);
	if (opt == NULL)
		return 0;

	switch (opt->type) {
		case option_boolean:
			rv = parse_boolean(line, opt->value);
			if (rv == 0) printf("Expected 'yes' or 'no'\n");
			break;
		case option_integer:
			rv = parse_integer(line, opt->value);
			if (rv == 0) printf("Expected number\n");
			break;
		case option_string:
			rv = parse_string(line, opt->value);
			if (rv == 0) printf("Expected string\n");
			break;
	}
		
	return rv;
}

struct config*
read_config(const char* path)
{
	FILE* f;
	char line[512];
	int linenumber = 0;
	struct config* c;

	if ((f = fopen(path, "r")) == NULL) {
		printf("Error: can't open config file %s\n", path);
		exit(1);
	}
	
	c = malloc(sizeof(struct config));
	memset(c, 0, sizeof(struct config));
	
	while (fgets(line, sizeof(line), f) != NULL) {
		if (line[0] != '#' && line[0] != '\n') {
			if (parse_line(line, c) == 0) {
				printf("Error parsing config file %s\n", path);
				printf("Please, check line %d\n", linenumber);
				exit(1);
			}
		}
		linenumber++;
	}
	
	fclose(f);
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

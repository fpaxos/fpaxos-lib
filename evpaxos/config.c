/*
	Copyright (c) 2013, University of Lugano
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
    	* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the copyright holders nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
*/


#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

struct address
{
	char* addr;
	int port;
};

struct evpaxos_config
{
	int proposers_count;
	int acceptors_count;
	struct address proposers[MAX_N_OF_PROPOSERS];
	struct address acceptors[MAX_N_OF_PROPOSERS];
};

enum option_type
{
	option_boolean,
	option_integer,
	option_string,
	option_verbosity
};

struct option
{
	const char* name;
	void* value;
	enum option_type type;
};

struct option options[] =
{
	{ "verbosity", &paxos_config.verbosity, option_verbosity },
	{ "learner-catch-up", &paxos_config.learner_catch_up, option_boolean },
	{ "proposer-timeout", &paxos_config.proposer_timeout, option_integer },
	{ "proposer-preexec-window", &paxos_config.proposer_preexec_window, option_integer },
	{ "bdb-sync", &paxos_config.bdb_sync, option_boolean },
	{ "bdb-cachesize", &paxos_config.bdb_cachesize, option_integer },
	{ "bdb-env-path", &paxos_config.bdb_env_path, option_string },
	{ "bdb-db-filename", &paxos_config.bdb_db_filename, option_string },
	{ "bdb-trash-files", &paxos_config.bdb_trash_files, option_boolean },
	{ 0 }
};

static int parse_line(struct evpaxos_config* c, char* line);
static void address_init(struct address* a, char* addr, int port);
static void address_free(struct address* a);
static struct sockaddr_in address_to_sockaddr(struct address* a);


struct evpaxos_config*
evpaxos_config_read(const char* path)
{
	FILE* f;
	char line[512];
	int linenumber = 0;
	struct evpaxos_config* c;

	if ((f = fopen(path, "r")) == NULL) {
		printf("Error: can't open config file %s\n", path);
		exit(1);
	}
	
	c = malloc(sizeof(struct evpaxos_config));
	memset(c, 0, sizeof(struct evpaxos_config));
	
	while (fgets(line, sizeof(line), f) != NULL) {
		if (line[0] != '#' && line[0] != '\n') {
			if (parse_line(c, line) == 0) {
				printf("Please, check line %d\n", linenumber);
				printf("Error parsing config file %s\n", path);
				exit(1);
			}
		}
		linenumber++;
	}
	
	fclose(f);
	return c;
}

void
evpaxos_config_free(struct evpaxos_config* config)
{
	int i;
	for (i = 0; i < config->proposers_count; ++i)
		address_free(&config->proposers[i]);
	for (i = 0; i < config->acceptors_count; ++i)
		address_free(&config->acceptors[i]);
	free(config);
}

int
paxos_proposer_count(struct evpaxos_config* config)
{
	return config->proposers_count;
}

struct sockaddr_in
evpaxos_proposer_address(struct evpaxos_config* config, int i)
{
	return address_to_sockaddr(&config->proposers[i]);
}
	
int
evpaxos_proposer_listen_port(struct evpaxos_config* config, int i)
{
	return config->proposers[i].port;
}

int 
evpaxos_acceptor_count(struct evpaxos_config* config)
{
	return config->acceptors_count;
}

struct sockaddr_in
evpaxos_acceptor_address(struct evpaxos_config* config, int i)
{
	return address_to_sockaddr(&config->acceptors[i]);
}

int
evpaxos_acceptor_listen_port(struct evpaxos_config* config, int i)
{
	return config->acceptors[i].port;
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

static int
parse_verbosity(char* str, int* verbosity)
{
	if (strcasecmp(str, "quiet") == 0) *verbosity = PAXOS_LOG_QUIET;
	else if (strcasecmp(str, "error") == 0) *verbosity = PAXOS_LOG_ERROR;
	else if (strcasecmp(str, "info") == 0) *verbosity = PAXOS_LOG_INFO;
	else if (strcasecmp(str, "debug") == 0) *verbosity = PAXOS_LOG_DEBUG;
	else return 0;
	return 1;
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
parse_line(struct evpaxos_config* c, char* line)
{
	int rv;
	char* tok;
	char* sep = " ";
	struct option* opt;
	
	line = strtrim(line);
	tok = strsep(&line, sep);
	
	if (strcasecmp(tok, "a") == 0) {
		if (c->acceptors_count >= MAX_N_OF_PROPOSERS) {
			printf("Number of acceptors exceded maximum of: %d\n",
				MAX_N_OF_PROPOSERS);
			return 0;
		}
		struct address* addr = &c->acceptors[c->acceptors_count++];
		return parse_address(line, addr);
	}
	
	if (strcasecmp(tok, "p") == 0) {
		if (c->proposers_count >= MAX_N_OF_PROPOSERS) {
			printf("Number of proposers exceded maximum of: %d\n",
				MAX_N_OF_PROPOSERS);
			return 0;
		}
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
		case option_verbosity:
			rv = parse_verbosity(line, opt->value);
			if (rv == 0) printf("Expected quiet, error, info, or debug\n");
			break;
	}
		
	return rv;
}

static void
address_init(struct address* a, char* addr, int port)
{
	a->addr = strdup(addr);
	a->port = port;
}

static void
address_free(struct address* a)
{
	free(a->addr);
}

static struct sockaddr_in
address_to_sockaddr(struct address* a) {
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(a->port);
	addr.sin_addr.s_addr = inet_addr(a->addr);
	return addr;
}

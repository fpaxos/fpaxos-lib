/*
 * Copyright (c) 2014-2015, University of Lugano
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
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

struct client_value
{
  struct timeval t;
  size_t size;
  char value[];
};

static int verbose = 1;

void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

static void
deliver(unsigned iid, char* value, size_t size, void* arg)
{
	if (verbose == 0)
		return;
	struct client_value* val = (struct client_value*)value;
	printf("%ld.%06d [%.16s] %ld bytes\n", val->t.tv_sec, val->t.tv_usec,
		val->value, (long)val->size);
}

static void
start_replica(int id, const char* config)
{
	struct event* sig;
	struct event_base* base;
	struct evpaxos_replica* replica;
	
	base = event_base_new();
	replica = evpaxos_replica_init(id, config, deliver, NULL, base);
	
	if (replica == NULL) {
		printf("Could not start the replica!\n");
		exit(1);
	}
	
	sig = evsignal_new(base, SIGINT, handle_sigint, base);
	evsignal_add(sig, NULL);
	event_base_dispatch(base);

	event_free(sig);
	evpaxos_replica_free(replica);
	event_base_free(base);
	
}

static void
usage(const char* prog)
{
	printf("Usage: %s id config [-h] [-s]\n", prog);
	printf(" -h, --help\tOutput this message\n");
	printf(" -s, --silent\tDon't print delivered messages\n");
	exit(1);
}

int
main(int argc, char const *argv[])
{
	if (argc < 3)
		usage(argv[0]);

	int i;
	for (i = 3; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			usage(argv[0]);
		else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--silent") == 0)
			verbose = 0;
		else
			usage(argv[0]);
	}
	
	int id = atoi(argv[1]);
	start_replica(id, argv[2]);
	
	return 0;
}

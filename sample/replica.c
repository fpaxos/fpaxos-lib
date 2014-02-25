/*
	Copyright (c) 2014, University of Lugano
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


#include <stdlib.h>
#include <stdio.h>
#include <evpaxos.h>
#include <signal.h>

void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

static void
deliver(char* value, size_t size, void* arg)
{
	printf("Delivered value: %s\n", value);
}

int
main(int argc, char const *argv[])
{
	struct event* sig;
	struct event_base* base;
	struct evpaxos_replica* replica;

	if (argc != 3) {
		printf("Usage: %s id config\n", argv[0]);
		exit(1);
	}
	
	signal(SIGPIPE, SIG_IGN);
	
	int id = atoi(argv[1]);
	base = event_base_new();
	replica = evpaxos_replica_init(id, argv[2], deliver, NULL, base);
	
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
	
	return 0;
}

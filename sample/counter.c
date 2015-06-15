/*
 * Copyright (c) 2014, University of Lugano
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <evpaxos.h>
#include <signal.h>

struct timeval count_interval = {0, 100000};

struct trim_info
{
	int count;
	int last_trim;
	int instances[16];
};

struct counter_replica
{
	int id;
	int count;
	unsigned instance_id;
	struct trim_info trim;
	struct event* client_ev;
	struct evpaxos_replica* paxos_replica;
};

static void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	event_base_loopexit(base, NULL);
}

static void
init_state(struct counter_replica* replica)
{
	char filename[128];
	replica->count = 0;
	replica->instance_id = 0;
	snprintf(filename, sizeof(filename), "state-%d", replica->id);
	FILE* f = fopen(filename, "r");
	if (f != NULL) {
		fscanf(f, "%d %d", &replica->count, &replica->instance_id);
		fclose(f);
	}
}

static void
checkpoint_state(struct counter_replica* replica)
{
	char filename[128];
	snprintf(filename, sizeof(filename), "state-%d", replica->id);
	FILE* f = fopen(filename, "w+");
	fprintf(f, "%d %d", replica->count, replica->instance_id);
	fclose(f);
}

static void
update_state(struct counter_replica* replica, unsigned iid)
{
	replica->count++;
	replica->instance_id = iid;
}

static void
update_trim_info(struct counter_replica* replica, int replica_id, int trim_id)
{
	if (trim_id <= replica->trim.instances[replica_id])
		return;
	int i, min = replica->trim.instances[0];
	replica->trim.instances[replica_id] = trim_id;
	for (i = 0; i < replica->trim.count; i++)
		if (replica->trim.instances[i] < min)
			min = replica->trim.instances[i];
	if (min > replica->trim.last_trim) {
		replica->trim.last_trim = min;
		evpaxos_replica_send_trim(replica->paxos_replica, min);
	}
}

static void
submit_trim(struct counter_replica* replica)
{
	char trim[64];
	snprintf(trim, sizeof(trim), "TRIM %d %d", replica->id, replica->instance_id);
	evpaxos_replica_submit(replica->paxos_replica, trim, strlen(trim)+1);
}

static void
on_deliver(unsigned iid, char* value, size_t size, void* arg)
{
	int replica_id, trim_id;
	struct counter_replica* replica = (struct counter_replica*)arg;
	
	if (sscanf(value, "TRIM %d %d", &replica_id, &trim_id) == 2) {
		update_trim_info(replica, replica_id, trim_id);
	} else {
		update_state(replica, iid);	
	}
	
	if (iid % 100 == 0) {
		checkpoint_state(replica);
		submit_trim(replica);
	}
}

static void
on_client_value(int fd, short ev, void* arg)
{
	struct counter_replica* replica = (struct counter_replica*)arg;
	evpaxos_replica_submit(replica->paxos_replica, "COUNT", 6);
	event_add(replica->client_ev, &count_interval);
}

static void
start_replica(int id, const char* config)
{
	struct event* sig;
	struct event_base* base;
	struct counter_replica replica;
	
	base = event_base_new();
	
	replica.id = id;	
	replica.paxos_replica = evpaxos_replica_init(id, config, on_deliver, 
		&replica, base);
	
	if (replica.paxos_replica == NULL) {
		printf("Failed to initialize paxos replica\n");
		exit(1);
	}
	
	init_state(&replica);
	evpaxos_replica_set_instance_id(replica.paxos_replica, replica.instance_id);
	
	memset(&replica.trim, 0, sizeof(struct trim_info));
	int replica_count = evpaxos_replica_count(replica.paxos_replica);
	replica.trim.count = replica_count;
	
	sig = evsignal_new(base, SIGINT, handle_sigint, base);
	evsignal_add(sig, NULL);
	
	replica.client_ev = evtimer_new(base, on_client_value, &replica);
	event_add(replica.client_ev, &count_interval);
	
	event_base_dispatch(base);
	
	event_free(sig);
	event_free(replica.client_ev);
	evpaxos_replica_free(replica.paxos_replica);
	event_base_free(base);
}

int
main(int argc, char const *argv[])
{
	int id;
	const char* config = "../paxos.conf";
	
	if (argc != 2 && argc != 3) {
		printf("Usage: %s id [path/to/paxos.conf]\n", argv[0]);
		exit(1);
	}
	
	id = atoi(argv[1]);
	if (argc >= 3)
		config = argv[2];
	
	signal(SIGPIPE, SIG_IGN);
	start_replica(id, config);

	return 0;
}

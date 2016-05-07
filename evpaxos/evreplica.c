/*
 * Copyright (c) 2013-2014, University of Lugano
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


#include "evpaxos_internal.h"
#include "message.h"
#include <stdlib.h>

struct evpaxos_replica
{
	struct peers* peers;
	struct evlearner* learner;
	struct evproposer* proposer;
	struct evacceptor* acceptor;
	deliver_function deliver;
	void* arg;
};

static void
evpaxos_replica_deliver(unsigned iid, char* value, size_t size, void* arg)
{
	struct evpaxos_replica* r = arg;
	evproposer_set_instance_id(r->proposer, iid);
	if (r->deliver)
		r->deliver(iid, value, size, r->arg);
}

struct evpaxos_replica*
evpaxos_replica_init(int id, const char* config_file, deliver_function f,
	void* arg, struct event_base* base)
{
	struct evpaxos_replica* r;
	struct evpaxos_config* config;
	r = malloc(sizeof(struct evpaxos_replica));
	
	config = evpaxos_config_read(config_file);
	
	r->peers = peers_new(base, config);
	peers_connect_to_acceptors(r->peers);
	
	r->acceptor = evacceptor_init_internal(id, config, r->peers);
	r->proposer = evproposer_init_internal(id, config, r->peers);
	r->learner  = evlearner_init_internal(config, r->peers,
		evpaxos_replica_deliver, r);
	r->deliver = f;
	r->arg = arg;

	int port = evpaxos_acceptor_listen_port(config, id);
	if (peers_listen(r->peers, port) == 0) {
		evpaxos_config_free(config);
		evpaxos_replica_free(r);
		return NULL;
	}
	
	evpaxos_config_free(config);
	return r;
}

void
evpaxos_replica_free(struct evpaxos_replica* r)
{
	if (r->learner)
		evlearner_free_internal(r->learner);
	evproposer_free_internal(r->proposer);
	evacceptor_free_internal(r->acceptor);
	peers_free(r->peers);
	free(r);
}

void
evpaxos_replica_set_instance_id(struct evpaxos_replica* r, unsigned iid)
{
	if (r->learner)
		evlearner_set_instance_id(r->learner, iid);
	evproposer_set_instance_id(r->proposer, iid);
}

static void
peer_send_trim(struct peer* p, void* arg)
{
	send_paxos_trim(peer_get_buffer(p), arg);
}

void
evpaxos_replica_send_trim(struct evpaxos_replica* r, unsigned iid)
{
	paxos_trim trim = {iid};
	peers_foreach_acceptor(r->peers, peer_send_trim, &trim);
}

void
evpaxos_replica_submit(struct evpaxos_replica* r, char* value, int size)
{
	int i;
	struct peer* p;
	for (i = 0; i < peers_count(r->peers); ++i) {
		p = peers_get_acceptor(r->peers, i);
		if (peer_connected(p)) {
			paxos_submit(peer_get_buffer(p), value, size);
			return;
		}
	}
}

int
evpaxos_replica_count(struct evpaxos_replica* r)
{
	return peers_count(r->peers);
}

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


#ifndef _EVPAXOS_H_
#define _EVPAXOS_H_

#include <sys/types.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#ifdef __cplusplus
extern "C" {
#endif

struct evlearner;
struct evproposer;
struct evacceptor;
struct evpaxos_replica;

/**
 * When starting a learner you must pass a callback to be invoked whenever
 * a value has been learned.
 */
typedef void (*deliver_function)(
	unsigned int,
	char* value,
	size_t size,
	void* arg);

/**
 * Create a Paxos replica, consisting of a collocated Acceptor, Proposer,
 * and Learner.
 *
 * @param id the id of the replica
 * @param config path a paxos config file
 * @param cb the callback function to be called whenever the learner delivers.
 * This paramater may be NULL, in which case no learner is initialized.
 * @param arg an optional argument that is passed to the callback
 * @param base the underlying event_base to be used
 *
 * @return a new evpaxos_replica on success, or NULL on failure.
 *
 * @see evpaxos_replica_free()
 */
struct evpaxos_replica* evpaxos_replica_init(int id, const char* config,
	deliver_function cb, void* arg, struct event_base* base);

/**
 * Destroy a Paxos replica and free all its memory.
 *
 * @param replica a evpaxos_replica to be freed
 */
void evpaxos_replica_free(struct evpaxos_replica* replica);

/**
 * Set the starting instance id of the given replica.
 * The replica will call the delivery function for instances larger than the
 * given instance id.
 *
 * @param iid the starting instance id
 */
void evpaxos_replica_set_instance_id(struct evpaxos_replica* replica,
	unsigned iid);

/**
 * Send a trim message to all acceptors/replicas. Acceptors will trim their log
 * up the the given instance id.
 *
 * @param iid trim instance id
 */
void evpaxos_replica_send_trim(struct evpaxos_replica* replica, unsigned iid);

/**
 * Used by replicas to submit values.
 */
void evpaxos_replica_submit(struct evpaxos_replica* replica,
	char* value, int size);

/**
 * Returns the number of replicas in the configuration.
 */
int evpaxos_replica_count(struct evpaxos_replica* replica);

/**
 * Initializes a learner with a given config file, a deliver callback,
 * an optional argument to that is passed to the callback, and
 * a libevent event_base.
 */
struct evlearner* evlearner_init(const char* config, deliver_function f,
	void* arg, struct event_base* base);

/**
 * Release the memory allocated by the learner
 */
void evlearner_free(struct evlearner* l);

/**
 * Set the starting instance id of the given learner.
 * The learner will call the delivery function for instances larger than the
 * given instance id.
 *
 * @param iid the starting instance id
 */
void evlearner_set_instance_id(struct evlearner* l, unsigned iid);

/**
 * Send a trim message to all acceptors/replicas. Acceptors will trim their log
 * up the the given instance id.
 *
 * @param iid trim instance id
 */
void evlearner_send_trim(struct evlearner* l, unsigned iid);

/**
 * Initializes a acceptor with a given id (which MUST be unique),
 * a config file and a libevent event_base.
 */
struct evacceptor* evacceptor_init(int id, const char* config,
	struct event_base* b);

/**
 * Frees the memory allocated by the acceptor.
 * This will also cleanly close the  * underlying storage.
 */
void evacceptor_free(struct evacceptor* a);

/**
 * Initializes a proposer with a given ID (which MUST be unique),
 * a config file and a libevent event_base.
 *
 * @param id a unique identifier, must be in the range [0...(MAX_N_OF_PROPOSERS-1)]
 */
struct evproposer* evproposer_init(int id, const char* config,
	struct event_base* b);

/**
 * Release the memory allocated by the proposer
 */
void evproposer_free(struct evproposer* p);

/**
 * This is a hint to the proposer to start from the given instance id.
 *
 * @param iid the starting instance id
 */
void evproposer_set_instance_id(struct evproposer* p, unsigned iid);

/**
 * Used by clients to submit values to proposers.
 */
void paxos_submit(struct bufferevent* bev, char* value, int size);

#ifdef __cplusplus
}
#endif

#endif

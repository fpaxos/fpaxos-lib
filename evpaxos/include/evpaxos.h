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


#ifndef _EVPAXOS_H_
#define _EVPAXOS_H_

#include "paxos.h"

#include <sys/types.h>
#include <stdint.h>
#include <event2/event.h>
#include <event2/bufferevent.h>


/* When starting a learner you must pass a callback to be invoked whenever
 * a value has been learned.*/
typedef void (*deliver_function)(char*, size_t, iid_t, ballot_t, int, void*);

/* Initializes a learner with a given config file, a deliver callback,
 * an optional argument to that is passed to the callback, and
 * a libevent event_base. */
struct evlearner*
evlearner_init(const char* config_file, deliver_function f, void* arg,
	struct event_base* base);

/* Release the memory allocated by the learner */
void
evlearner_free(struct evlearner* l);

/* Initializes a acceptor with a given id (which MUST be unique),
 * a config file and a libevent event_base.
 * id -> Must be in the range [0...(MAX_N_OF_ACCEPTORS-1)] */
struct evacceptor*
evacceptor_init(int id, const char* config, struct event_base* b);

/* Frees the memory allocated by the acceptor. This will also cleanly close the  * underlying storage. */
int
evacceptor_free(struct evacceptor* a);

/* Initializes a proposer with a given ID (which MUST be unique), 
 * a config file and a libevent event_base.
 * id -> Must be in the range [0...(MAX_N_OF_PROPOSERS-1)] */
struct evproposer*
evproposer_init(int id, const char* config, struct event_base* b);

/* Release the memory allocated by the proposer */
void
evproposer_free(struct evproposer* p);

/* Used by clients to submit values to proposers. */
void
paxos_submit(struct bufferevent* bev, char* value, int size);

#endif

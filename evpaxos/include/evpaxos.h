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

#include "libpaxos.h"

#include <sys/types.h>
#include <stdint.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

#define MAX_ADDR 10

typedef struct address_t {
	char* address_string;
	int port;
} address;

struct config
{
	int proposers_count;
	int acceptors_count;
	address proposers[MAX_ADDR];
	address acceptors[MAX_ADDR];
};

/* 
	When starting a learner you must pass a function to be invoked whenever
	a value is delivered.
	This defines the type of such function.
	Example: 
	void my_deliver_fun(char * value, size_t size, iid_t iid, ballot_t ballot, int proposer) {
		...
	}
*/
typedef void (* deliver_function)(char*, size_t, iid_t, ballot_t, int, void*);

/*
    Starts a learner and returns when the initialization is complete.
    Return value is 0 if successful
    f -> A deliver_function invoked when a value is delivered.
         This argument cannot be NULL
         It's called by an internal thread therefore:
         i)  it must be quick
         ii) you must synchronize/lock externally if this function touches data
             shared with some other thread (i.e. the one that calls learner init)
    cif -> A custom_init_function invoked by the internal libevent thread, 
           invoked when the normal learner initialization is completed
           Can be used to add other events to the existing event loop.
           It's ok to pass NULL if you don't need it.
           cif has to return -1 for error and 0 for success
*/
struct evlearner*
evlearner_init(const char* config_file, deliver_function f, void* arg,
	struct event_base* base);

void evlearner_free(struct evlearner* l);

/*
	Starts an acceptor and returns when the initialization is complete.
	Return value is 0 if successful
	acceptor_id -> Must be in the range [0...(N_OF_ACCEPTORS-1)]
*/
struct evacceptor*
evacceptor_init(int id, const char* config, struct event_base* b);

/*
	Shuts down the acceptor in the current process.
	It may take a few seconds to complete since the DB needs to be closed.
*/
int
evacceptor_free(struct evacceptor* a);

/*
	Starts a proposer with the given ID (which MUST be unique).
	Return value is 0 if successful
	proposer_id -> Must be in the range [0...(MAX_N_OF_PROPOSERS-1)]
*/
struct evproposer*
evproposer_init(int id, const char* config, struct event_base* b);


void evproposer_free(struct evproposer* p);


/*
	Function used for submitting values to a proposer.
*/
void
paxos_submit(struct bufferevent* bev, char* value, int size);

#endif

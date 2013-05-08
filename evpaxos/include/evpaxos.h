#ifndef _EVPAXOS_H_
#define _EVPAXOS_H_

#include "libpaxos.h"
#include "libpaxos_messages.h"
#include "config_reader.h"

#include <sys/types.h>
#include <stdint.h>
#include <event2/event.h>
#include <event2/bufferevent.h>

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

struct evlearner* 
evlearner_init_conf(struct config* c, deliver_function f, void* arg, 
	struct event_base* base);


/*
	Starts an acceptor and returns when the initialization is complete.
	Return value is 0 if successful
	acceptor_id -> Must be in the range [0...(N_OF_ACCEPTORS-1)]
*/
struct evacceptor*
evacceptor_init(int id, const char* config, struct event_base* b);

/*
	Starts an acceptor that instead of creating a clean DB,
	tries to recover from an existing one.
	Return value is 0 if successful
*/
struct evacceptor*
evacceptor_init_recover(int id, const char* config, struct event_base* b);

/*
	Shuts down the acceptor in the current process.
	It may take a few seconds to complete since the DB needs to be closed.
*/
int
evacceptor_exit(struct evacceptor* a);

/*
	Starts a proposer with the given ID (which MUST be unique).
	Return value is 0 if successful
	proposer_id -> Must be in the range [0...(MAX_N_OF_PROPOSERS-1)]
*/
struct evproposer*
evproposer_init(int id, const char* config, struct event_base* b);

/*
	Function used for submitting values to a proposer.
*/
void
paxos_submit(struct bufferevent* bev, char* value, int size);

#endif

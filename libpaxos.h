#ifndef _LIBPAXOS_H_
#define _LIBPAXOS_H_
#include <sys/types.h>
#include <stdint.h>
#include "paxos_config.h"

/* 
    The maximum size that can be submitted by a client.
    Set MAX_UDP_MSG_SIZE in config file to reflect your network MTU.
    Max packet size minus largest header possible
    (should be accept_ack_batch+accept_ack, around 30 bytes)
*/
#define PAXOS_MAX_VALUE_SIZE (MAX_UDP_MSG_SIZE - 40)

/* 
    Alias for instance identificator and ballot number.
*/
typedef unsigned int ballot_t;
typedef uint32_t iid_t;

/* 
    When starting a learner you must pass a function to be invoked whenever
    a value is delivered.
    This defines the type of such function.
    Example: 
    void my_deliver_fun(char * value, size_t size, iid_t iid, ballot_t ballot, int proposer) {
        ...
    }
*/
typedef void (* deliver_function)(char*, size_t, iid_t, ballot_t, int);


/* 
    When starting a learner you may pass a function to be invoked 
    within libevent, after the normal learner initialization.
    (so that new events/timeouts can be added)
    This defines the type of such function.
    Example: 
    void int my_custom_init() {
        ...
    }
*/
typedef int (* custom_init_function)(void);

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
int learner_init(deliver_function f, custom_init_function cif);

/*
    Starts an acceptor and returns when the initialization is complete.
    Return value is 0 if successful
    acceptor_id -> Must be in the range [0...(N_OF_ACCEPTORS-1)]
*/
int acceptor_init(int acceptor_id);

/*
    Starts an acceptor that instead of creating a clean DB,
    tries to recover from an existing one.
    Return value is 0 if successful
*/
int acceptor_init_recover(int acceptor_id);

/*
    Shuts down the acceptor in the current process.
    It may take a few seconds to complete since the DB needs to be closed.
*/
//FIXME should delegate close to libevent thread
int acceptor_exit();

/*
    Starts a proposer with the given ID (which MUST be unique).
    Return value is 0 if successful
    proposer_id -> Must be in the range [0...(MAX_N_OF_PROPOSERS-1)]
*/
int proposer_init(int proposer_id);

/*
    Like proposer_init with a custom initialization function.
    Allows to start custom behavior on top of the proposer libevent loop.
    proposer_id -> Must be in the range [0...(MAX_N_OF_PROPOSERS-1)]
    cif -> A custom_init_function invoked by the internal libevent thread, 
           invoked when the normal proposer initialization is completed
           Can be used to add other events to the existing event loop.
           It's ok to pass NULL if you don't need it.
           cif has to return -1 for error and 0 for success    
*/
int proposer_init_cif(int proposer_id, custom_init_function cif);

/*
    This is returned to the when creating a new submit handle
*/
typedef struct paxos_submit_handle_t {
    void * sendbuf;
} paxos_submit_handle;

/*
    Creates a new handle for this client to submit values.
    Different threads in a process can have their personal handle
    or share a common one (locking is up to you!)
*/
paxos_submit_handle * pax_submit_handle_init();

/*
    This call sends a value to the current leader and returns immediately.
    There is no guarantee that the value even reached the leader.
*/
int pax_submit_nonblock(paxos_submit_handle * h, char * value, size_t val_size);

void pax_submit_sharedmem(char* value, size_t val_size);

#endif /* _LIBPAXOS_H_ */

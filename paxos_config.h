#ifndef _PAXOS_CONFIG_H_
#define _PAXOS_CONFIG_H_

/*** PROTOCOL SETTINGS ***/

/* 
    Number of instances prepared by the leader in advance
    (executes phase1 even if no value is yet present for phase 2).
    Setting too high may produce lots of timeout, since the acceptors
    take a while to answer.
    Should be a multiple of PROPOSER_P2_CONCURRENCY (double or more)
    MUST be less than PROPOSER_ARRAY_SIZE (half or less)
*/
#define PROPOSER_PREEXEC_WIN_SIZE 128


/* 
    The maximum number of proposers must be fixed beforehand
    (this is because of unique ballot generation).
    The proposers must be started with different IDs.
    This number MUST be a power of 10.
*/
#define MAX_N_OF_PROPOSERS  10

/* 
    The number of acceptors must be fixed beforehand.
    The acceptors must be started with different IDs.
*/
#define N_OF_ACCEPTORS  3

/* 
    Rule for calculating whether the number of accept_ack messages (phase 2b) 
    is sufficient to declare the instance closed and deliver 
    the corresponding value. i.e.:
    Paxos     -> ((int)(N_OF_ACCEPTORS/2))+1;
    FastPaxos -> 1 + (int)((double)(N_OF_ACCEPTORS*2)/3);
*/

#define QUORUM (((int)(N_OF_ACCEPTORS/2))+1)


/*** ACCEPTORS DB SETTINGS ***/

/*
    Setting for how 'strict' the durability of acceptors should be.
    From weaker and faster to stricter and durable.
    Acceptors use Berkeley DB as a stable storage layer.
    
    No durability on crash:
    0   -> Uses in-memory storage
        Writes to disk if the memory cache is full.
    10  -> Transactional Data Store (write, in-memory logging)
        (DB_LOG_IN_MEMORY)
    Durability despite process crash:
    11  -> Transactional Data Store (write, no-sync on commit)
        (DB_TXN_NOSYNC)
    12 ->Transactional Data Store (write, write-no-sync on commit)
        (DB_TXN_WRITE_NOSYNC)
    Durability despite OS crash:
    13  -> Transactional Data Store (write, sync on commit)
        (default transactional storage)
    20  -> "Manually" call DB->sync before answering requests
        (may corrupt database file on crash)
*/
#define DURABILITY_MODE 0

/*
    This defines where the acceptors create their database files.
    A._DB_PATH is the absolute path of a directory.
    If it does not exist will be created. Unless starting in recovery mode, 
    the content of the directory will deleted.
    A._DB_FNAME is the name for the db file.
    %d is replaced by 'acceptor_id'
    The concatenation of those MUST fit in 512 chars
*/
#define ACCEPTOR_DB_PATH "/tmp/acceptor_%d", acceptor_id
#define ACCEPTOR_DB_FNAME "acc_db_%d.bdb", acceptor_id

/*
    Acceptor's access method on their underlying DB.
    Only DB_BTREE and DB_RECNO are available, other methods
    requires additional configuration and do not fit well.
    Acceptors use Berkeley DB as a stable storage layer.
*/
// #define ACCEPTOR_ACCESS_METHOD DB_BTREE
#define ACCEPTOR_ACCESS_METHOD DB_RECNO


/*** NETWORK SETTINGS ***/

/* 
  Maximum size of UDP messages for the current network and host OS.
  Check carefully, if not set properly, message send will fail.
*/
#define MAX_UDP_MSG_SIZE 7500


/*
  If defined, UDP sockets created (to send) are non-blocking.
  The send call may return before data is actually transmitted.
  Comment the definition below to make the send blocking.
*/
// #define PAXOS_UDP_SEND_NONBLOCK

/*** STRUCTURES SETTINGS ***/

/*
  Size of the in-meory table of instances for the proposer.
  MUST be bigger than PROPOSER_PREEXEC_WIN_SIZE (double or more)
  MUST be a power of 2
*/
#define PROPOSER_ARRAY_SIZE 2048

/* 
  Size of the in-meory table of instances for the learner.
  MUST be bigger than PROPOSER_PREEXEC_WIN_SIZE (double or more)
  MUST be a power of 2
*/
#define LEARNER_ARRAY_SIZE 2048


/*** DEBUGGING SETTINGS ***/

/*
  Verbosity of the library
  0 -> off (prints only errors)
  1 -> verbose 
  3 -> debug
  
  //TODO set most useful as VRB
*/
#define VERBOSITY_LEVEL 0

/*
  If PAXOS_DEBUG_MALLOC is defined, it turns on
  malloc debugging to detect memory leaks.
  The trace filename must be defined anyway
*/
// #define PAXOS_DEBUG_MALLOC
#define MALLOC_TRACE_FILENAME "malloc_debug_trace.txt"

#endif

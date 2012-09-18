#ifndef PAXOS_CONFIG_H_24LVFLYO
#define PAXOS_CONFIG_H_24LVFLYO

/*** PROTOCOL SETTINGS ***/

/* 
    Number of instances prepared by the leader in advance
    (executes phase1 even if no value is yet present for phase 2).
    Setting too high may produce lots of timeout, since the acceptors
    take a while to answer.
    Should be a multiple of PROPOSER_P2_CONCURRENCY (double or more)
    MUST be less than PROPOSER_ARRAY_SIZE (half or less)
*/
#define PROPOSER_PREEXEC_WIN_SIZE 300

/* 
    Number of instances that are concurrently opened by the leader.
    If this is 1, the leader won't try to send an accept for
    instance i+1 until instance i is closed.
    If more than 1, FIFO order of values is not granted.
    MUST be smaller than PROPOSER_PREEXEC_WIN_SIZE (half or less)
*/
#define PROPOSER_P2_CONCURRENCY 3

/* 
    The timeout for prepare requests. If too high, the leader takes a while
    to realize the timeout. If too low, requests expire too early.
    To calibrate, set to an high value (seconds), activate 
    LEADER_EVENTS_UPDATE_INTERVAL and observe the number of timeouts.
    Then lower this value until only a few occour.
    The most important factors are PROPOSER_PREEXEC_WIN_SIZE and
    the latency of the network.
    Unit is microseconds - i.e. 1500000 = 1.5 secs
*/
#define P1_TIMEOUT_INTERVAL 30000

/* 
    The timeout for accept requests. If too high, the leader takes a while
    to realize the timeout. If too low, requests expire too early.
    To calibrate, set to an high value (seconds), activate 
    LEADER_EVENTS_UPDATE_INTERVAL and observe the number of timeouts.
    Then lower this value until only a few occour.
    The most important factors are the size of submitted values and
    the latency of the network.
    Unit is microseconds - i.e. 1500000 = 1.5 secs
*/
#define P2_TIMEOUT_INTERVAL 35000

/* 
    How frequently should the leader proposer try to open new instances.
    (P2 execution does not rely exclusively on this peridic check, 
    new ones are opened also when some old instance is closed/delivered).
    Unit is microseconds - i.e. 1000 = 1ms */
#define P2_CHECK_INTERVAL 1000

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

/* 
    This option makes each acceptor a learner too.
    Delivered values are written in stable storage.
    This makes recovery and catch-up easier in some situations,
    since the other learners (and therefore proposers too) can learn
    the final value for an instance with a single message.
    Undefine to disable.
*/
// #define ACCEPTOR_UPDATE_ON_DELIVER

/*
    Periodically (every ACCEPTOR_REPEAT_INTERVAL seconds) the acceptor
    repeats the latest (by instance id) accept acknowledged. 
    This is useful to keep the learners updated in very low-traffic 
    situations.
*/
#define ACCEPTOR_REPEAT_INTERVAL 3

/*
    Periodically the learner checks for "holes": that is cases where
    instance i is closed but it cannot be delivered since instances i-1 
    or i-2 are not closed yet.
    If some hole is detected, it asks the acceptors to repeat
    their accepted values for those instances.
    Unit is microseconds - i.e. 1500000 = 1.5 secs

*/
#define LEARNER_HOLECHECK_INTERVAL 500000

/*
    The maximum size of the pending list of values in the leader proposer.
    It has to be limited since client my retry to submit too early, if they send 
    at a rate higher than the proposer can digest, the list grows to infinity
*/
#define LEADER_MAX_QUEUE_LENGTH 50


/*** FAILURE DETECTOR SETTINGS ***/

/*
    How frequently each proposer sends it's 'alive' message
    to the failure oracle.
    Unit is microseconds.
*/
#define FAILURE_DETECTOR_PING_INTERVAL 3000000

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
  Multicast <address, port> for the respective groups
  The first three are used by the protocol. Only on the fourth one
  the client is allowed to send submitted values.
  The fifth and sixth are reserved for leader election and failure detection
*/
#define PAXOS_LEARNERS_NET  "239.0.0.1", 6001
#define PAXOS_ACCEPTORS_NET "239.1.0.1", 6002
#define PAXOS_PROPOSERS_NET "239.2.0.1", 6003
#define PAXOS_SUBMIT_NET    "239.3.0.1", 6004
#define PAXOS_ORACLE_NET    "239.4.0.1", 6005
#define PAXOS_PINGS_NET     "239.5.0.1", 6006

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
   How frequently the current leader prints the status report
   The counters printed are particularly useful for calibration
   (i.e. number of p1/p2 timeouts)
   10000000 = print every 10 sec
   Undefine to disable.
*/
#define LEADER_EVENTS_UPDATE_INTERVAL 10000000

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


#endif /* end of include guard: PAXOS_CONFIG_H_24LVFLYO */

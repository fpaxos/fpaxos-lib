#ifndef LIBPAXOS_MESSAGES_H_HP8GZLGD
#define LIBPAXOS_MESSAGES_H_HP8GZLGD

/*
    Paxos message types
*/

typedef enum pax_msg_code_e {
    prepare_reqs=1,     //Phase 1a, P->A
    prepare_acks=2,     //Phase 1b, A->P
    accept_reqs=4,      //Phase 2a, P->A
    accept_acks=8,      //Phase 2b, A->L
    repeat_reqs=16,     //For progress, L -> A
    submit=32,          //Clients to leader
    leader_announce=64, //Oracle to proposers
    alive_ping=65       //Proposers to oracle
} paxos_msg_code;

typedef struct paxos_msg_t {
    size_t data_size; //Size of 'data' in bytes
    paxos_msg_code type;
    char data[0];
} paxos_msg;
#define PAXOS_MSG_SIZE(M) (M->data_size + sizeof(paxos_msg))

/* 
    Paxos protocol messages
*/
//Phase 1a, prepare request
typedef struct prepare_req_t {
    iid_t iid;
    ballot_t ballot;
} prepare_req;
#define PREPARE_REQ_SIZE(M) (sizeof(prepare_req))

//Phase 1b, prepare acknowledgement
typedef struct prepare_ack_t {
    iid_t iid;
    ballot_t ballot;
    ballot_t value_ballot;
    size_t value_size;
    char value[0];
} prepare_ack;
#define PREPARE_ACK_SIZE(M) (M->value_size + sizeof(prepare_ack)) 

//Phase 2a, accept request
typedef struct accept_req_t {
    iid_t iid;
    ballot_t ballot;
    size_t value_size;
    char value[0];
} accept_req;
#define ACCEPT_REQ_SIZE(M) (M->value_size + sizeof(accept_req))

//Phase 2b, accept acknowledgement
typedef struct accept_ack_t {
    iid_t       iid;
    ballot_t    ballot;
    ballot_t    value_ballot;
    short int   is_final;
    size_t      value_size;
    char        value[0];
} accept_ack;
#define ACCEPT_ACK_SIZE(M) (M->value_size + sizeof(accept_ack))


/* 
    Batches to send multiple paxos messages in a single packet
*/

typedef struct prepare_req_batch_t {
    short int count;
    short int proposer_id;
    prepare_req prepares[0];
} prepare_req_batch;
#define PREPARE_REQ_BATCH_SIZE(M) (sizeof(prepare_req_batch) + (M->count*sizeof(prepare_req)))


typedef struct prepare_ack_batch_t {
    short int acceptor_id;
    short int count;
    char data[0];
} prepare_ack_batch;
size_t prepare_ack_batch_size_calc(prepare_ack_batch * pab);
#define PREPARE_ACK_BATCH_SIZE(M) (prepare_ack_batch_size_calc(M))


typedef struct accept_req_batch_t {
    short int count;
    short int proposer_id;
    char data[0];
} accept_req_batch;
size_t accept_req_batch_size_calc(accept_req_batch * aab);
#define ACCEPT_REQ_BATCH_SIZE(M) (accept_req_batch_size_calc(M))

typedef struct accept_ack_batch_t {
    short int   acceptor_id;
    short int   count;
    char        data[0];
} accept_ack_batch;
size_t accept_ack_batch_size_calc(accept_ack_batch * aab);
#define ACCEPT_ACK_BATCH_SIZE(M) (accept_ack_batch_size_calc(M))

typedef struct repeat_req_batch_t {
    short int count;
    iid_t requests[0];
} repeat_req_batch;
#define REPEAT_REQ_BATCH_SIZE(B) (sizeof(repeat_req_batch) + (sizeof(iid_t) * B->count))

/* 
    Failure detection/leader election messages
*/
typedef struct leader_announce_msg_t {
    short int current_leader;
} leader_announce_msg;

typedef struct alive_ping_msg_t {
    short int proposer_id;
    long unsigned int sequence_number;
} alive_ping_msg;



#endif /* end of include guard: LIBPAXOS_MESSAGES_H_HP8GZLGD */

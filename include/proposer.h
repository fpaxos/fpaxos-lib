#ifndef _PROPOSER_H_
#define _PROPOSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct proposer;

struct proposer* proposer_new(int id, int instances);
void proposer_propose(struct proposer* s, paxos_msg* msg);

// phase 1
void proposer_prepare(struct proposer* s, iid_t* iout, ballot_t* bout);
void proposer_receive_prepare(struct proposer* s, prepare_ack* ack);

// phase 2
int proposer_accept(struct proposer* s, iid_t* iout, ballot_t* bout, paxos_msg** vout);
void proposer_receive_accept(struct proposer* s, accept_ack* ack);

#ifdef __cplusplus
}
#endif

#endif

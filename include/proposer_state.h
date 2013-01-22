#ifndef _PROPOSER_STATE_H_
#define _PROPOSER_STATE_H_

#include "libpaxos_priv.h"
#include "libpaxos_messages.h"

struct proposer_state;

struct proposer_state* proposer_state_new(int id, int instances);
void proposer_state_propose(struct proposer_state* s, paxos_msg* msg);

// phase 1
void proposer_state_prepare(struct proposer_state* s, iid_t* iout, ballot_t* bout);
void proposer_state_receive_prepare(struct proposer_state* s, prepare_ack* ack);

// phase 2
int proposer_state_accept(struct proposer_state* s, iid_t* iout, ballot_t* bout, paxos_msg** vout);
void proposer_state_receive_accept(struct proposer_state* s, accept_ack* ack);

#endif

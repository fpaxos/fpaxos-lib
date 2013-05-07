#ifndef _LEARNER_STATE_H_
#define _LEARNER_STATE_H_

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct learner_state;

struct learner_state* learner_state_new(int instances);
void learner_state_receive_accept(struct learner_state* s, accept_ack* ack);
accept_ack* learner_state_deliver_next(struct learner_state* s);

#endif

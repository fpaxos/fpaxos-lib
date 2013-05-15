#ifndef _LEARNER_H_
#define _LEARNER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct learner;

struct learner* learner_new(int instances, int recover);
void learner_receive_accept(struct learner* s, accept_ack* ack);
accept_ack* learner_deliver_next(struct learner* s);
int learner_has_holes(struct learner* s, iid_t* from, iid_t* to);

#ifdef __cplusplus
}
#endif

#endif

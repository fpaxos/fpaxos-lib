#ifndef _PROPOSER_H_
#define _PROPOSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct proposer;

struct proposer* proposer_new(int id, int instances);
void proposer_propose(struct proposer* p, char* value, size_t size);

// phase 1
prepare_req proposer_prepare(struct proposer* p);
prepare_req* proposer_receive_prepare_ack(struct proposer* p, prepare_ack* ack);

// phase 2
accept_req* proposer_accept(struct proposer* p);
prepare_req* proposer_receive_accept_ack(struct proposer* p, accept_ack* ack);

#ifdef __cplusplus
}
#endif

#endif

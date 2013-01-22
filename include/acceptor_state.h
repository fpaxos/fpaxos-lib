#ifndef _ACCEPTOR_STATE_H_
#define _ACCEPTOR_STATE_H_

#include "libpaxos_priv.h"

struct acceptor_state;

struct acceptor_state* acceptor_state_new(int id);
int acceptor_state_delete(struct acceptor_state* s);
acceptor_record* acceptor_state_receive_prepare(struct acceptor_state* s, prepare_req* req);
acceptor_record* acceptor_state_receive_accept(struct acceptor_state* s, accept_req* req);

#endif

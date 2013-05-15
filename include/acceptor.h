#ifndef _ACCEPTOR_H_
#define _ACCEPTOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct acceptor;

struct acceptor* acceptor_new(int id);
int acceptor_delete(struct acceptor* s);
acceptor_record* acceptor_receive_prepare(struct acceptor* s, prepare_req* req);
acceptor_record* acceptor_receive_accept(struct acceptor* s, accept_req* req);
acceptor_record* acceptor_receive_repeat(struct acceptor* a, iid_t iid);

#ifdef __cplusplus
}
#endif

#endif

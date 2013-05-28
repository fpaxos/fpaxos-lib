#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "libpaxos.h"
#include "libpaxos_messages.h"


/**
	Use the #define below to switch between different implementations
    of acceptor storage:

    ACCEPTOR_STORAGE_MEM
    ACCEPTOR_STORAGE_BDB
*/
#define ACCEPTOR_STORAGE_MEM


struct storage;

struct storage* storage_open(int acceptor_id, int recovery);
int storage_close(struct storage* s);
void storage_tx_begin(struct storage* s);
void storage_tx_commit(struct storage* s);
acceptor_record* storage_get_record(struct storage* s, iid_t iid);
acceptor_record* storage_save_accept(struct storage* s, accept_req * ar);
acceptor_record* storage_save_prepare(struct storage* s, prepare_req * pr, acceptor_record * rec);
acceptor_record* storage_save_final_value(struct storage* s, char * value, size_t size, iid_t iid, ballot_t ballot);

#endif

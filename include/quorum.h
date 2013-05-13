#ifndef _QUORUM_H_
#define _QUORUM_H_

#include "paxos_config.h"

struct quorum
{
	int count;
	int quorum;
	int acceptor_ids[N_OF_ACCEPTORS];
};

void quorum_init(struct quorum *q, int quorum);
int quorum_add(struct quorum* q, int id);
int quorum_reached(struct quorum* q);

#endif

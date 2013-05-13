#include "quorum.h"
#include <string.h>

void
quorum_init(struct quorum* q, int quorum)
{
	int i;
	q->count = 0;
	q->quorum = quorum;
	for (i = 0; i < N_OF_ACCEPTORS; ++i)
		q->acceptor_ids[i] = 0;
}

int
quorum_add(struct quorum* q, int id)
{
	if (q->acceptor_ids[id] == 0) {
		q->count++;
		q->acceptor_ids[id] = 1;
		return 1;
	}
	return 0;
}

int
quorum_reached(struct quorum* q)
{
	return (q->count >= q->quorum);
}

//
// Created by Michael Davis on 07/02/2020.
//

#ifndef LIBPAXOS_EPOCH_QUORUM_H
#define LIBPAXOS_EPOCH_QUORUM_H

#include <khash.h>

KHASH_MAP_INIT_INT(epoch_quorum, struct quorum*)

struct epoch_quorum
{
    khash_t(epoch_quorum) epoch_quorums;
};

void epoch_quorum_init(struct epoch_quorum *q, int acceptors, int quorum_size);
void epoch_quorum_clear(struct epoch_quorum* q);
void epoch_quorum_resize(struct epoch_quorum* q, int epoch_quorum_size);
void epoch_quorum_destroy(struct epoch_quorum* q);
int epoch_quorum_add(struct epoch_quorum* q, uint32_t epoch, int id); // add acceptor id to epoch quorum
void epoch_quorum_reached(struct epoch_quorum* q, uint32_t* epoch, int* quorum_reached);  // returns whether or not a quorum is reached and which epoch the quorum is on if so


#endif //LIBPAXOS_EPOCH_QUORUM_H

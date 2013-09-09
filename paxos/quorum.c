/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "paxos.h"
#include "quorum.h"
#include <stdlib.h>
#include <string.h>


void
quorum_init(struct quorum* q, int acceptors)
{
	q->acceptors = acceptors;
	q->quorum = paxos_quorum(acceptors);
	q->acceptor_ids = malloc(sizeof(int) * q->acceptors);
	quorum_clear(q);
}

void
quorum_clear(struct quorum* q)
{
	q->count = 0;
	memset(q->acceptor_ids, 0, sizeof(int) * q->acceptors);
}

void
quorum_destroy(struct quorum* q)
{
	free(q->acceptor_ids);
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

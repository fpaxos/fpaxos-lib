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


#ifndef _PROPOSER_H_
#define _PROPOSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "paxos.h"

struct proposer;
struct timeout_iterator;

struct proposer* proposer_new(int id, int acceptors);
void proposer_free(struct proposer* p);
void proposer_propose(struct proposer* p, const char* value, size_t size);
int proposer_prepared_count(struct proposer* p);

// phase 1
void proposer_prepare(struct proposer* p, paxos_prepare* out);
int proposer_receive_promise(struct proposer* p, paxos_promise* ack, 
	paxos_prepare* out);

// phase 2
int proposer_accept(struct proposer* p, paxos_accept* out);
int proposer_receive_accepted(struct proposer* p, paxos_accepted* ack, 
	paxos_prepare* out);

// timeouts
struct timeout_iterator* proposer_timeout_iterator(struct proposer* p);
int timeout_iterator_prepare(struct timeout_iterator* iter, paxos_prepare* out);
int timeout_iterator_accept(struct timeout_iterator* iter, paxos_accept* out);
void timeout_iterator_free(struct timeout_iterator* iter);

#ifdef __cplusplus
}
#endif

#endif

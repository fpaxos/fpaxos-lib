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

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct proposer;

struct proposer* proposer_new(int id);
void proposer_propose(struct proposer* p, char* value, size_t size);
int proposer_prepared_count(struct proposer* p);

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

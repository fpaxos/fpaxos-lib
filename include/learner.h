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


#ifndef _LEARNER_H_
#define _LEARNER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct learner;

struct learner* learner_new(int instances, int recover);
void learner_receive_accept(struct learner* s, accept_ack* ack);
accept_ack* learner_deliver_next(struct learner* s);
int learner_has_holes(struct learner* s, iid_t* from, iid_t* to);

#ifdef __cplusplus
}
#endif

#endif

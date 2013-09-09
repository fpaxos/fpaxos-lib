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

#include "paxos.h"

struct learner;

struct learner* learner_new(int acceptors);
void learner_free(struct learner* l);
void learner_receive_accepted(struct learner* l, paxos_accepted* ack);
int learner_deliver_next(struct learner* l, paxos_accepted* out);
int learner_has_holes(struct learner* l, iid_t* from, iid_t* to);

#ifdef __cplusplus
}
#endif

#endif

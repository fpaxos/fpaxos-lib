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

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


#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "libpaxos.h"
#include "libpaxos_messages.h"

struct storage;

struct storage* storage_open(int acceptor_id);
int storage_close(struct storage* s);
void storage_tx_begin(struct storage* s);
void storage_tx_commit(struct storage* s);
acceptor_record* storage_get_record(struct storage* s, iid_t iid);
acceptor_record* storage_save_accept(struct storage* s, accept_req * ar);
acceptor_record* storage_save_prepare(struct storage* s, prepare_req * pr, acceptor_record * rec);
acceptor_record* storage_save_final_value(struct storage* s, char * value, size_t size, iid_t iid, ballot_t ballot);
iid_t storage_get_max_iid(struct storage * s);

#endif

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


#ifndef _XDR_H_
#define _XDR_H_

#include "paxos.h"
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/rpc.h>

bool_t xdr_paxos_value(XDR* xdrs, paxos_value* objp);
bool_t xdr_paxos_prepare(XDR* xdrs, paxos_prepare* objp);
bool_t xdr_paxos_promise(XDR* xdrs, paxos_promise* objp);
bool_t xdr_paxos_accept(XDR* xdrs, paxos_accept* objp);
bool_t xdr_paxos_accepted(XDR* xdrs, paxos_accepted* objp);
bool_t xdr_paxos_repeat(XDR *xdrs, paxos_repeat *objp);
bool_t xdr_paxos_client_value(XDR *xdrs, paxos_client_value *objp);
bool_t xdr_paxos_message_type(XDR *xdrs, paxos_message_type *objp);
bool_t xdr_paxos_message(XDR *xdrs, paxos_message *objp);

#endif

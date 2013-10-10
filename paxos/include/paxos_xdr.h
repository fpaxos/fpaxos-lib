/*
	Copyright (c) 2013, University of Lugano
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
    	* Redistributions of source code must retain the above copyright
		  notice, this list of conditions and the following disclaimer.
		* Redistributions in binary form must reproduce the above copyright
		  notice, this list of conditions and the following disclaimer in the
		  documentation and/or other materials provided with the distribution.
		* Neither the name of the copyright holders nor the
		  names of its contributors may be used to endorse or promote products
		  derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
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

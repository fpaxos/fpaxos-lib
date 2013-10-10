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


#include "message.h"
#include "paxos_xdr.h"
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/rpc.h>
#include <event2/bufferevent.h>


static int
send_message(struct bufferevent* bev, paxos_message* msg)
{
	XDR xdr;
	uint32_t size;
	char buffer[PAXOS_MAX_VALUE_SIZE];
	xdrmem_create(&xdr, buffer, PAXOS_MAX_VALUE_SIZE, XDR_ENCODE);
	if (!xdr_paxos_message(&xdr, msg)) {
		paxos_log_error("Error while encoding paxos message");
		xdr_destroy(&xdr);
		return 0;
	}
	size = htonl(xdr_getpos(&xdr));
	bufferevent_write(bev, &size, sizeof(uint32_t));
	bufferevent_write(bev, buffer, xdr_getpos(&xdr));
	xdr_destroy(&xdr);
	return 1;
}

void
send_paxos_prepare(struct bufferevent* bev, paxos_prepare* p)
{
	paxos_message msg = {
		.type = PAXOS_PREPARE,
		.paxos_message_u.prepare = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send prepare for iid %d ballot %d", p->iid, p->ballot);
}

void
send_paxos_promise(struct bufferevent* bev, paxos_promise* p)
{
	paxos_message msg = {
		.type = PAXOS_PROMISE,
		.paxos_message_u.promise = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send promise for iid %d ballot %d", p->iid, p->ballot);
}

void 
send_paxos_accept(struct bufferevent* bev, paxos_accept* p)
{
	paxos_message msg = {
		.type = PAXOS_ACCEPT,
		.paxos_message_u.accept = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send accept for iid %d ballot %d", p->iid, p->ballot);
}

void
send_paxos_accepted(struct bufferevent* bev, paxos_accepted* p)
{	
	paxos_message msg = {
		.type = PAXOS_ACCEPTED,
		.paxos_message_u.accepted = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send accepted for inst %d ballot %d", p->iid, p->ballot);
}

void
send_paxos_repeat(struct bufferevent* bev, paxos_repeat* p)
{
	paxos_message msg = {
		.type = PAXOS_REPEAT,
		.paxos_message_u.repeat = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send repeat for inst %d-%d", p->from, p->to);
}

void
paxos_submit(struct bufferevent* bev, char* data, int size)
{
	paxos_message msg = {
		.type = PAXOS_CLIENT_VALUE,
		.paxos_message_u.client_value.value.paxos_value_len = size,
		.paxos_message_u.client_value.value.paxos_value_val = data };
	send_message(bev, &msg);
}

static int
decode_paxos_message(struct evbuffer* in, paxos_message* out, size_t size)
{
	XDR xdr;
	char* buffer = (char*)evbuffer_pullup(in, size);
	xdrmem_create(&xdr, buffer, size, XDR_DECODE);
	memset(out, 0, sizeof(paxos_message));
	int rv = xdr_paxos_message(&xdr, out);
	if (!rv) paxos_log_error("Error while decoding paxos message!");
	xdr_destroy(&xdr);
	return rv;
}

int
recv_paxos_message(struct evbuffer* in, paxos_message* out)
{
	uint32_t msg_size;
	size_t buffer_len = evbuffer_get_length(in);
	
	if (buffer_len <= sizeof(uint32_t))
		return 0;
	
	evbuffer_copyout(in, &msg_size, sizeof(uint32_t));
	msg_size = ntohl(msg_size);
	
	if (buffer_len < (msg_size + sizeof(uint32_t)))
		return 0;
	
	if (msg_size > PAXOS_MAX_VALUE_SIZE) {
		evbuffer_drain(in, msg_size);
		paxos_log_error("Discarding message of size %ld. Maximum is %d",
			msg_size, PAXOS_MAX_VALUE_SIZE);
		return 0;
	}
	
	evbuffer_drain(in, sizeof(uint32_t));
	int rv = decode_paxos_message(in, out, msg_size);
	evbuffer_drain(in, msg_size);

	return rv;
}

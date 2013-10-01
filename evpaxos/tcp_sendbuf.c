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


#include "libpaxos_messages.h"
#include "tcp_sendbuf.h"
#include <event2/bufferevent.h>


static void
add_paxos_header(struct bufferevent* bev, paxos_msg_code c, size_t s)
{
	paxos_msg m;
	m.data_size = s;
	m.type = c;
	bufferevent_write(bev, &m, sizeof(paxos_msg));
}

void 
sendbuf_add_prepare_req(struct bufferevent* bev, prepare_req* pr)
{
	size_t s = PREPARE_REQ_SIZE(pr);
	add_paxos_header(bev, prepare_reqs, s);
	bufferevent_write(bev, pr, s);
	paxos_log_debug("Send prepare iid: %d ballot: %d", pr->iid, pr->ballot);
}

void
sendbuf_add_prepare_ack(struct bufferevent* bev, acceptor_record* rec)
{
	size_t s;
	prepare_ack pa;
	
	pa.acceptor_id = rec->acceptor_id;
	pa.iid = rec->iid;
	pa.ballot = rec->ballot;
	pa.value_ballot = rec->value_ballot;
	pa.value_size = rec->value_size;
	
	s = PREPARE_ACK_SIZE((&pa));
	add_paxos_header(bev, prepare_acks, s);
	bufferevent_write(bev, &pa, sizeof(prepare_ack));
	if (pa.value_size > 0)
		bufferevent_write(bev, rec->value, rec->value_size);
	paxos_log_debug("Send prepare ack for inst %d ballot %d", rec->iid,
		rec->ballot);
}

void 
sendbuf_add_accept_req(struct bufferevent* bev, accept_req* ar)
{
	size_t s = ACCEPT_REQ_SIZE(ar);
	add_paxos_header(bev, accept_reqs, s);
	bufferevent_write(bev, ar, s);
	paxos_log_debug("Send accept req for inst %d ballot %d", ar->iid, 
		ar->ballot);
}

void
sendbuf_add_accept_ack(struct bufferevent* bev, accept_ack* aa)
{	
	size_t s = ACCEPT_ACK_SIZE(aa);
	add_paxos_header(bev, accept_acks, s);
	bufferevent_write(bev, aa, s);
	paxos_log_debug("Send accept ack for inst %d ballot %d", aa->iid,
		aa->ballot);
}

void
sendbuf_add_repeat_req(struct bufferevent* bev, iid_t iid)
{
	add_paxos_header(bev, repeat_reqs, sizeof(iid_t));
	bufferevent_write(bev, &iid, sizeof(iid_t));
	paxos_log_debug("Send repeat request for inst %d", iid);
}

void
paxos_submit(struct bufferevent* bev, char* value, int size)
{
	add_paxos_header(bev, submit, size);
	bufferevent_write(bev, value, size);
}

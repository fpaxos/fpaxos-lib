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
	LOG(VRB, ("sending prepare iid: %d ballot: %d\n", pr->iid, pr->ballot));
	size_t s = PREPARE_REQ_SIZE(pr);
	add_paxos_header(bev, prepare_reqs, s);
	bufferevent_write(bev, pr, s);
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

	LOG(VRB, ("sending prepare ack iid: %d ballot %d\n", rec->iid, 
		rec->ballot));

	s = PREPARE_ACK_SIZE((&pa));
	add_paxos_header(bev, prepare_acks, s);
	bufferevent_write(bev, &pa, sizeof(prepare_ack));
	if (pa.value_size > 0)
		bufferevent_write(bev, rec->value, rec->value_size);
}

void 
sendbuf_add_accept_req(struct bufferevent* bev, accept_req* ar)
{
	LOG(VRB, ("sending accept req for inst %d ballot %d\n", 
		ar->iid, ar->ballot));
	size_t s = ACCEPT_REQ_SIZE(ar);
	add_paxos_header(bev, accept_reqs, s);
	bufferevent_write(bev, ar, s);
}

void
sendbuf_add_accept_ack(struct bufferevent* bev, accept_ack* aa)
{	
	LOG(VRB, ("sending accept ack for inst %d ballot %d\n", 
		aa->iid, aa->ballot));

	size_t s = ACCEPT_ACK_SIZE(aa);
	add_paxos_header(bev, accept_acks, s);
	bufferevent_write(bev, aa, s);
}

void
sendbuf_add_repeat_req(struct bufferevent* bev, iid_t iid)
{
	LOG(VRB, ("sending repeat request for inst %d\n", iid));
	add_paxos_header(bev, repeat_reqs, sizeof(iid_t));
	bufferevent_write(bev, &iid, sizeof(iid_t));
}

void
paxos_submit(struct bufferevent* bev, char* value, int size)
{
	add_paxos_header(bev, submit, size);
	bufferevent_write(bev, value, size);
}

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

	LOG(VRB, ("sending prepare ack iid: %d ballot %d\n", rec->iid, rec->ballot));

	s = PREPARE_ACK_SIZE((&pa));
	add_paxos_header(bev, prepare_acks, s);
	bufferevent_write(bev, &pa, s);
	if (pa.value_size > 0)
		bufferevent_write(bev, rec->value, rec->value_size);
}

void 
sendbuf_add_accept_req(struct bufferevent* bev, iid_t iid,
	ballot_t ballot, paxos_msg* payload)
{
	size_t s;
	accept_req ar;
	s = sizeof(accept_req) + payload->data_size;

	LOG(VRB, ("sending accept req for inst %d ballot %d\n", iid, ballot));
	
	ar.iid = iid;
	ar.ballot = ballot;
	ar.value_size = payload->data_size;
	
	add_paxos_header(bev, accept_reqs, s);
	bufferevent_write(bev, &ar, sizeof(accept_req));
	bufferevent_write(bev, payload->data, payload->data_size);
}

void
sendbuf_add_accept_ack(struct bufferevent* bev, accept_ack* aa)
{
	size_t s;
	
	LOG(VRB, ("sending accept ack for inst %d ballot %d\n", 
		aa->iid, aa->ballot));

	s = ACCEPT_ACK_SIZE(aa);
	add_paxos_header(bev, accept_acks, s);
	bufferevent_write(bev, aa, s);
}

void
paxos_submit(struct bufferevent* bev, char* value, int size)
{
	add_paxos_header(bev, submit, size);
	bufferevent_write(bev, value, size);
}

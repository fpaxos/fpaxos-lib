#ifndef _TCP_SENDBUF_H_
#define _TCP_SENDBUF_H_

#include "evpaxos.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <event2/bufferevent.h>

void sendbuf_add_prepare_ack(struct bufferevent* bev, acceptor_record* rec);
void sendbuf_add_prepare_req(struct bufferevent* bev, iid_t iid, ballot_t ballot);
void sendbuf_add_accept_req(struct bufferevent* bev, iid_t iid, ballot_t ballot, paxos_msg* payload);
void sendbuf_add_accept_ack(struct bufferevent* bev, acceptor_record* rec);

#endif

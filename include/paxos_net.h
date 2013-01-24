#ifndef _PAXOS_NET_H_
#define _PAXOS_NET_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <event2/bufferevent.h>

#include "libpaxos_priv.h"
#include "libpaxos_messages.h"

void sendbuf_add_prepare_ack(struct bufferevent* bev, acceptor_record* rec, int id);
void sendbuf_add_prepare_req(struct bufferevent* bev, iid_t iid, ballot_t ballot);
void sendbuf_add_accept_req(struct bufferevent* bev, iid_t iid, ballot_t ballot, paxos_msg* payload);
void sendbuf_add_accept_ack(struct bufferevent* bev, acceptor_record* rec);
void sendbuf_add_submit_val(struct bufferevent* bev, char* value, int size);

#endif

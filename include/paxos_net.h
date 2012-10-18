#ifndef _PAXOS_NET_H_
#define _PAXOS_NET_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <event2/bufferevent.h>

#include "libpaxos_priv.h"
#include "libpaxos_messages.h"
#include "tcp_receiver.h"


typedef struct connection_t {
	int sock;
	struct sockaddr_in addr;
} connection;


#define MAX_CONNECTIONS 10
typedef struct udp_send_buffer_t {
    int dirty;
    char buffer[MAX_UDP_MSG_SIZE];
	int connections_count;
	connection connections[MAX_CONNECTIONS];
} udp_send_buffer;


typedef struct udp_receiver_t {
    int sock;
    struct sockaddr_in addr;
    char recv_buffer[MAX_UDP_MSG_SIZE];
} udp_receiver;


//Creates a new non-blocking sender for the given address/port pairs
//Returns NULL for error
udp_send_buffer * sendbuf_new(int count, address* addresses);

void sendbuf_clear(udp_send_buffer * sb, paxos_msg_code type, short int sender_id);
void sendbuf_flush(udp_send_buffer * sb);


void sendbuf_add_repeat_req(udp_send_buffer * sb, iid_t iid);

void sendbuf_add_prepare_ack(struct bufferevent* bev, acceptor_record * rec, int id);
void sendbuf_add_prepare_req(struct bufferevent* bev, iid_t iid, ballot_t ballot);
void sendbuf_add_accept_req(struct bufferevent* bev, iid_t iid, ballot_t ballot, paxos_msg* payload);
void sendbuf_add_accept_ack(struct tcp_receiver* r, acceptor_record* rec);
void sendbuf_add_submit_val(struct bufferevent* bev, char* value, int size);

udp_receiver * udp_receiver_blocking_new(address* a);
udp_receiver * udp_receiver_new(address* a);
int udp_read_next_message(udp_receiver * recv_info);
int udp_receiver_destroy(udp_receiver * rec);

void sendbuf_send_ping(udp_send_buffer * sb, short int proposer_id, long unsigned int sequence_number);
void sendbuf_send_leader_announce(udp_send_buffer * sb, short int leader_id);


void print_paxos_msg(paxos_msg * msg);

#endif

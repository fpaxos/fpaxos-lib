#ifndef PAXOS_UDP_H_X98E254H
#define PAXOS_UDP_H_X98E254H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "libpaxos_priv.h"
#include "libpaxos_messages.h"

typedef struct udp_send_buffer_t {
    int sock;
    struct sockaddr_in addr;
    int dirty;
    // size_t bufsize;
    char buffer[MAX_UDP_MSG_SIZE];
} udp_send_buffer;

typedef struct udp_receiver_t {
    int sock;
    struct sockaddr_in addr;
    char recv_buffer[MAX_UDP_MSG_SIZE];
} udp_receiver;


udp_send_buffer * udp_sendbuf_new(char* address_string, int port);
void sendbuf_clear(udp_send_buffer * sb, paxos_msg_code type, short int sender_id);
void sendbuf_flush(udp_send_buffer * sb);


void sendbuf_add_repeat_req(udp_send_buffer * sb, iid_t iid);
void sendbuf_add_accept_ack(udp_send_buffer * sb, acceptor_record * rec);
void sendbuf_add_prepare_req(udp_send_buffer * sb, iid_t iid, ballot_t ballot);
void sendbuf_add_prepare_ack(udp_send_buffer * sb, acceptor_record * rec);
void sendbuf_add_accept_req(udp_send_buffer * sb, iid_t iid, ballot_t ballot, char * value, size_t val_size);
void sendbuf_add_submit_val(udp_send_buffer * sb, char * value, size_t val_size);


udp_receiver * udp_receiver_blocking_new(char* address_string, int port);
udp_receiver * udp_receiver_new(char* address_string, int port);
int udp_read_next_message(udp_receiver * recv_info);
int udp_receiver_destroy(udp_receiver * rec);

void sendbuf_send_ping(udp_send_buffer * sb, short int proposer_id, long unsigned int sequence_number);
void sendbuf_send_leader_announce(udp_send_buffer * sb, short int leader_id);


void print_paxos_msg(paxos_msg * msg);

#endif /* end of include guard: PAXOS_UDP_H_X98E254H */

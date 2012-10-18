#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <unistd.h>

#include "libpaxos_priv.h"
#include "paxos_net.h"

//Calculate size of dynamic structure by iterating
size_t prepare_ack_batch_size_calc(prepare_ack_batch * pab) {
    size_t total_size = 0;
    size_t offset = 0;
    short int i;
    
    //Iterate over prepare_ack in batch
    prepare_ack * pa;
    for(i = 0; i < pab->count; i++) {
        pa = (prepare_ack*) &pab->data[offset];
        offset += PREPARE_ACK_SIZE(pa);
    }
    
    //Add size of batch header
    total_size = sizeof(prepare_ack_batch) + offset;

    return total_size;
    
}

//Calculate size of dynamic structure by iterating
size_t accept_req_batch_size_calc(accept_req_batch * arb) {
    size_t total_size = 0;
    size_t offset = 0;
    short int i;
    
    //Iterate over accept_req in batch
    accept_req * ar;
    for(i = 0; i < arb->count; i++) {
        ar = (accept_req*) &arb->data[offset];
        offset += ACCEPT_REQ_SIZE(ar);
    }
    
    //Add size of batch header
    total_size = sizeof(accept_req_batch) + offset;

    return total_size;
    
}

//Calculate size of dynamic structure by iterating
size_t accept_ack_batch_size_calc(accept_ack_batch * aab) {
    size_t total_size = 0;
    size_t offset = 0;
    short int i;
    
    //Iterate over accept_ack in batch
    accept_ack * aa;
    for(i = 0; i < aab->count; i++) {
        aa = (accept_ack*) &aab->data[offset];
        offset += ACCEPT_ACK_SIZE(aa);
    }
    
    //Add size of batch header
    total_size = sizeof(accept_ack_batch) + offset;

    return total_size;
    
}

// Do some "superficial" check on the received message, 
// for example size should match the expected, proposer/acceptor IDs
// should be within given bounds, etc
// Returns 0 for valid, -1 for bad message
static int validate_paxos_msg(paxos_msg * m, size_t msg_size) {
    size_t expected_size = sizeof(paxos_msg);
    
    //Msg size should match received size
    if (m->data_size + sizeof(paxos_msg) != msg_size) {
        printf("Invalid message, declared size:%lu received size:%u\n", \
            (m->data_size + sizeof(paxos_msg)), (unsigned int)msg_size);
        return -1;
    }
    
    switch(m->type) {
        case prepare_reqs: {
            prepare_req_batch * prb = (prepare_req_batch *)m->data;
            //Proposer id out of bounds
            if(prb->proposer_id < 0 || prb->proposer_id >= MAX_N_OF_PROPOSERS) {
                printf("Invalida proposer id:%d\n", prb->proposer_id);
                return -1;
            }
            expected_size += PREPARE_REQ_BATCH_SIZE(prb);
        }
        break;

        case prepare_acks: {
            prepare_ack_batch * pab = (prepare_ack_batch *)m->data;
            //Acceptor id out of bounds
            if(pab->acceptor_id < 0 || pab->acceptor_id >= N_OF_ACCEPTORS) {
                printf("Invalida acceptor id:%d\n", pab->acceptor_id);
                return -1;
            }
            expected_size += PREPARE_ACK_BATCH_SIZE(pab);
        }
        break;

        case accept_reqs: {
            accept_req_batch * arb = (accept_req_batch *)m->data;
            //Proposer id out of bounds
            if(arb->proposer_id < 0 || arb->proposer_id >= MAX_N_OF_PROPOSERS) {
                printf("Invalida proposer id:%d\n", arb->proposer_id);
                return -1;
            }
            expected_size += ACCEPT_REQ_BATCH_SIZE(arb);
        }
        break;
        
        case accept_acks: {
            accept_ack_batch * aab = (accept_ack_batch *)m->data;
            //Acceptor id out of bounds
            if(aab->acceptor_id < 0 || aab->acceptor_id >= N_OF_ACCEPTORS) {
                printf("Invalida acceptor id:%d\n", aab->acceptor_id);
                return -1;
            }
            expected_size += ACCEPT_ACK_BATCH_SIZE(aab);
        }
        break;
        
        case repeat_reqs: {
            repeat_req_batch * rrb = (repeat_req_batch *)m->data;
            expected_size += REPEAT_REQ_BATCH_SIZE(rrb);
        }
        break;

        case submit: {
            expected_size += m->data_size;
        }
        break;
        
        case alive_ping: {
            expected_size += sizeof(alive_ping_msg);
        }
        break;

        case leader_announce: {
            expected_size += sizeof(leader_announce_msg);
        }
        break;
        
        default: {
            printf("Unknow paxos message type:%d\n", m->type);
            return -1;
        }
        
        //Checks message data field size based on message type
        if(msg_size != expected_size) {
            printf("Invalid size for msg_type:%d declared:%u received:%u\n", \
                m->type, (unsigned int)expected_size, (unsigned int)msg_size);
        }
    }
    return 0;
}

void print_paxos_msg(paxos_msg * msg) {
    printf("[msg=%d size:%lu+%lu ", 
        msg->type, sizeof(paxos_msg), msg->data_size);
    
    int i;
    size_t offset = 0;
    switch(msg->type) {
        
        case prepare_reqs: {
            prepare_req_batch * prb = (prepare_req_batch *)msg->data;
            printf("(prepare request batch)\n");
            printf(" sender proposer:%d, count:%d\n", 
                prb->proposer_id, prb->count);
            prepare_req * pr;
            for(i = 0; i < prb->count; i++) {
                pr = (prepare_req *) &prb->prepares[i];
                printf("\n (%d) iid:%u bal:%u ", 
                    (int)i, pr->iid, pr->ballot);
            }

        }
        break;

        case prepare_acks: {
            prepare_ack_batch * pab = (prepare_ack_batch *)msg->data;
            printf("(prepare acknowledgement batch)\n");
            printf(" sender acceptor:%d, count:%d\n", 
                pab->acceptor_id, pab->count);
            prepare_ack * pa;
            for(i = 0; i < pab->count; i++) {
                pa = (prepare_ack *) &pab->data[offset];
                printf("\n (%p)(%d) iid:%u bal:%u vbal:%u val_size:%lu", 
                    (void*)pa, (int)i, pa->iid, pa->ballot, 
                    pa->value_ballot, pa->value_size);
                offset += PREPARE_ACK_SIZE(pa);
            }
        }
        break;

        case accept_reqs: {
            accept_req_batch * arb = (accept_req_batch *)msg->data;
            printf("(accept request batch)\n");
            printf(" sender proposer:%d, count:%d\n", 
                arb->proposer_id, arb->count);
            
            accept_req * ar;
            for(i = 0; i < arb->count; i++) {
                ar = (accept_req *) &arb->data[offset];
                printf("\n (%d) iid:%u bal:%u val_size:%lu", 
                    (int)i, ar->iid, ar->ballot, ar->value_size);
                offset += ACCEPT_REQ_SIZE(ar);
            }
        }
        break;
        
        case accept_acks: {
            accept_ack_batch * aab = (accept_ack_batch *)msg->data;
            printf("(accept acknowledgement batch)\n");
            printf(" sender acceptor:%d, count:%d\n", 
                aab->acceptor_id, aab->count);
            accept_ack * aa;
            for(i = 0; i < aab->count; i++) {
                aa = (accept_ack *) &aab->data[offset];
                printf("\n (%d) iid:%u bal:%u vbal:%u val_size:%lu", 
                    (int)i, aa->iid, aa->ballot, 
                    aa->value_ballot, aa->value_size);
                offset += ACCEPT_ACK_SIZE(aa);
            }
        }
        break;
        
        case repeat_reqs: {
            repeat_req_batch * rrb = (repeat_req_batch *)msg->data;
            printf("(repeat request batch)\n");
            printf(" count:%d\n", rrb->count);
            for(i = 0; i < rrb->count; i++) {
                printf("\n (%d) iid:%u ", 
                    (int)i, rrb->requests[i]);
            }
        }
        break;

        default: {
            printf("Unknow paxos message type:%d\n", msg->type);
        }
    }
    printf("]\n");
}

//Creates a new blocking UDP multicast receiver for the given address/port
udp_receiver * udp_receiver_blocking_new(address* a) {
    udp_receiver * rec = PAX_MALLOC(sizeof(udp_receiver));

    struct ip_mreq mreq;
    
    memset(&mreq, '\0', sizeof(struct ip_mreq));
    // Set up socket 
    rec->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (rec->sock < 0) {
        perror("receiver socket");
        return NULL;
    }
    
    // Set to reuse address   
    int activate = 1;
    if (setsockopt(rec->sock, SOL_SOCKET, SO_REUSEADDR, &activate, sizeof(int)) != 0) {
        perror("setsockopt, setting SO_REUSEADDR");
        return NULL;
    }

    // Set up membership to multicast group 
    mreq.imr_multiaddr.s_addr = inet_addr(a->address_string);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(rec->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
        perror("setsockopt, setting IP_ADD_MEMBERSHIP");
        return NULL;
    }

    // Set up address 
    struct sockaddr_in * addr_p = &rec->addr;
    addr_p->sin_addr.s_addr = inet_addr(a->address_string);
    if (addr_p->sin_addr.s_addr == INADDR_NONE) {
        printf("Error setting receiver->addr\n");
        return NULL;
    }
    addr_p->sin_family = AF_INET;
    addr_p->sin_port = htons((uint16_t)a->port);   

    // Bind the socket 
    if (bind(rec->sock, (struct sockaddr *) &rec->addr, sizeof(struct sockaddr_in)) != 0) {
        perror("bind");
        return NULL;
    }
    return rec;
}

//Creates a new non-blocking UDP multicast receiver for the given address/port
udp_receiver * udp_receiver_new(address* a) {

    udp_receiver * rec;
    rec = udp_receiver_blocking_new(a);

    if(rec == NULL) {
        return NULL;
    }

    // Set non-blocking 
    int flag = fcntl(rec->sock, F_GETFL);
    if(flag < 0) {
        perror("fcntl1");
        return NULL;
    }
    flag |= O_NONBLOCK;
    if(fcntl(rec->sock, F_SETFL, flag) < 0) {
        perror("fcntl2");
        return NULL;
    }
    
    LOG(DBG, ("Socket %d created for address %s:%d (receive mode)\n", rec->sock, a->address_string, a->port));
    return rec;
}

//Destroys the given UDP receiver
int udp_receiver_destroy(udp_receiver * rec) {
    int ret = 0;
    
    // Close the socket
    if (close(rec->sock) != 0) {
        printf("Error closing socket\n");
        perror("close");
        ret = -1;
    }
    LOG(DBG, ("Socket %d closed\n", rec->sock));
    
    //Free the structure
    PAX_FREE(rec);
    return ret;
}

//Tries to read the next message from socket into the local buffer.
// This function is registered with libevent and invoked automatically 
// when a new message is available in the system buffer.
// Returns 0 for a valid message, -1 otherwise
int udp_read_next_message(udp_receiver * recv_info) {
    
    //Get the message
    socklen_t addrlen = sizeof(struct sockaddr);
    int msg_size = recvfrom(recv_info->sock,    //Socket to read from
        recv_info->recv_buffer,                 //Where to store the msg
        MAX_UDP_MSG_SIZE,                       //Size of buffer
        MSG_WAITALL,                            //Get the entire message
        (struct sockaddr *)&recv_info->addr,    //Address
        &addrlen);                              //Address length

    //Error in recvfrom
    if (msg_size < 0) {
        perror("recvfrom");
        sleep(1);
        return -1;
    }
    
    return validate_paxos_msg((paxos_msg*)recv_info->recv_buffer, msg_size);
}

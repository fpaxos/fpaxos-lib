#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <assert.h>

#include "libpaxos_priv.h"
#include "paxos_udp.h"
#include "acceptor_stable_storage.h"

/*
    This module automates sending of UDP messages, when data is added to an "open" message,
    it will check to see if it fits. If it doesn't it will automatically close and send 
    the current message and then create a new one in which the data is added.
*/

//Prepares the send buffer for sending a message of the specific type
void sendbuf_clear(udp_send_buffer * sb, paxos_msg_code type, short int sender_id) {

    sb->dirty = 0;

    paxos_msg * m = (paxos_msg *) &sb->buffer;
    m->type = type;
    
    //Initial size, paxos header not included
    m->data_size = 0;
    
    switch(type) {
        
        //Proposer
        case prepare_reqs: {
            m->data_size = sizeof(prepare_req_batch);
            prepare_req_batch * prb = (prepare_req_batch *)&m->data;
            prb->count = 0;
            prb->proposer_id = sender_id;
        } break;
        
        case accept_reqs: {
            m->data_size = sizeof(accept_req_batch);
            accept_req_batch * arb = (accept_req_batch *)&m->data;
            arb->count = 0;
            arb->proposer_id = sender_id;
        } break;

        //Acceptor
        case prepare_acks: {
            m->data_size = sizeof(prepare_ack_batch);
            prepare_ack_batch * pab = (prepare_ack_batch *)&m->data;
            pab->count = 0;
            pab->acceptor_id = sender_id;
        } break;
        
        case accept_acks: {
            m->data_size += sizeof(accept_ack_batch);
            accept_ack_batch * aab = (accept_ack_batch *)&m->data;
            aab->count = 0;
            aab->acceptor_id = sender_id;
        } break;
        
        //Learner
        case repeat_reqs: {
            m->data_size += sizeof(repeat_req_batch);
            repeat_req_batch * rrb = (repeat_req_batch *)&m->data;
            rrb->count = 0;    
        } break;

        //Client
        case submit: {
            m->data_size += 0;
        } break;
            
        default: {            
            printf("Invalid message type %d for sendbuf_clear!\n", 
                type);
        }
    }
}

//Adds a prepare_req to the current message (a prepare_req_batch)
void sendbuf_add_prepare_req(udp_send_buffer * sb, iid_t iid, ballot_t ballot) {
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    assert(m->type == prepare_reqs);

    prepare_req_batch * prb = (prepare_req_batch *)&m->data;
    
    if(PAXOS_MSG_SIZE(m) + sizeof(prepare_req) >= MAX_UDP_MSG_SIZE) {
        // Next propose_req to add does not fit, flush the current 
        // message before adding it
        sendbuf_flush(sb);
        sendbuf_clear(sb, m->type, prb->proposer_id);
    }
    
    prepare_req * pr = (prepare_req *)&prb->prepares[prb->count];
    pr->iid = iid;
    pr->ballot = ballot;
    prb->count += 1;

    sb->dirty = 1;
    m->data_size += sizeof(prepare_req);
    
}

//Adds a prepare_ack to the current message (a prepare_ack_batch)
void sendbuf_add_prepare_ack(udp_send_buffer * sb, acceptor_record * rec) {
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    assert(m->type == prepare_acks);    

    prepare_ack_batch * pab = (prepare_ack_batch *)&m->data;
    
    size_t pa_size = (sizeof(prepare_ack) + rec->value_size);
    if(PAXOS_MSG_SIZE(m) + pa_size >= MAX_UDP_MSG_SIZE) {
        // Next propose_ack to add does not fit, flush the current 
        // message before adding it
        stablestorage_tx_end();
        sendbuf_flush(sb);
        sendbuf_clear(sb, m->type, pab->acceptor_id);
        stablestorage_tx_begin();
    }
    
    prepare_ack * pa = (prepare_ack *)&m->data[m->data_size];
    pa->iid = rec->iid;
    pa->ballot = rec->ballot;
    pa->value_ballot = rec->value_ballot;
    pa->value_size = rec->value_size;
    
    //If there's no value this copies 0 bytes!
    memcpy(pa->value, rec->value, rec->value_size);
    sb->dirty = 1;
    m->data_size += pa_size;
    pab->count += 1;

}

void sendbuf_add_accept_req(udp_send_buffer * sb, iid_t iid, ballot_t ballot, char * value, size_t val_size) {
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    assert(m->type == accept_reqs);

    accept_req_batch * arb = (accept_req_batch *)&m->data;
    
    size_t ar_size = sizeof(accept_req) + val_size;
    
    if(PAXOS_MSG_SIZE(m) + ar_size >= MAX_UDP_MSG_SIZE) {
        // Next accept to add does not fit, flush the current 
        // message before adding it
        sendbuf_flush(sb);
        sendbuf_clear(sb, m->type, arb->proposer_id);
    }

    accept_req * ar = (accept_req *)&m->data[m->data_size];
    ar->iid = iid;
    ar->ballot = ballot;
    ar->value_size = val_size;
    memcpy(ar->value, value, val_size);
    
    sb->dirty = 1;
    m->data_size += ar_size;
    arb->count += 1;
    
}


//Adds an accept_ack to the current message (an accept_ack_batch)
void sendbuf_add_accept_ack(udp_send_buffer * sb, acceptor_record * rec) {    
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    assert(m->type == accept_acks);

    accept_ack_batch * aab = (accept_ack_batch *)&m->data;
    
    size_t aa_size = ACCEPT_ACK_SIZE(rec);
    
    if(PAXOS_MSG_SIZE(m) + aa_size >= MAX_UDP_MSG_SIZE) {
        // Next accept to add does not fit, flush the current 
        // message before adding it
        stablestorage_tx_end();
        sendbuf_flush(sb);
        sendbuf_clear(sb, m->type, aab->acceptor_id);
        stablestorage_tx_begin();
    }
    

    accept_ack * aa = (accept_ack *)&m->data[m->data_size];
    memcpy(aa, rec, aa_size);
    
    sb->dirty = 1;
    m->data_size += aa_size;
    aab->count += 1;
    
}

//Adds an repeat_req to the current message (an repeat_req_batch)
void sendbuf_add_repeat_req(udp_send_buffer * sb, iid_t iid) {
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    assert(m->type == repeat_reqs);

    if(PAXOS_MSG_SIZE(m) + sizeof(iid_t) >= MAX_UDP_MSG_SIZE) {
        // Next iid to add does not fit, flush the current 
        // message before adding it
        sendbuf_flush(sb);
        sendbuf_clear(sb, m->type, -1);
    }
    
    sb->dirty = 1;
    m->data_size += sizeof(iid_t);
    
    repeat_req_batch * rrb = (repeat_req_batch *)&m->data;
    rrb->requests[rrb->count] = iid;
    rrb->count += 1;
}

void sendbuf_add_submit_val(udp_send_buffer * sb, char * value, size_t val_size) {
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    assert(m->type == submit);

    sb->dirty = 1;
    m->data_size += val_size;
    memcpy(m->data, value, val_size);
}

void sendbuf_send_ping(udp_send_buffer * sb, short int proposer_id, long unsigned int sequence_number) {
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    sb->dirty = 1;
    m->type = alive_ping;
    m->data_size = sizeof(alive_ping_msg);
    alive_ping_msg * ap = (alive_ping_msg *) m->data;
    ap->proposer_id = proposer_id;
    ap->sequence_number = sequence_number;
    sendbuf_flush(sb);
}

void sendbuf_send_leader_announce(udp_send_buffer * sb, short int leader_id) {
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    sb->dirty = 1;
    m->type = leader_announce;
    m->data_size = sizeof(leader_announce_msg);
    leader_announce_msg * la = (leader_announce_msg *) m->data;
    la->current_leader = leader_id;
    sendbuf_flush(sb);
    
}



//Flushes (sends) the current message in buffer, 
// but only if the 'dirty' flag is set
void sendbuf_flush(udp_send_buffer * sb) {
    int cnt;
    
    //The dirty field is used to determine if something 
    // is in the buffer waiting to be sent
    if(!sb->dirty) {
        return;
    }
    
    //Send the current message in buffer
    paxos_msg * m = (paxos_msg *) &sb->buffer;
    cnt = sendto(sb->sock,              //Sock
        sb->buffer,                     //Data
        PAXOS_MSG_SIZE(m),              //Data size
        0,                              //Flags
        (struct sockaddr *)&sb->addr,   //Addr
        sizeof(struct sockaddr_in));    //Addr size
        
    if (cnt != (int)PAXOS_MSG_SIZE(m) || cnt == -1) {
        perror("failed to send message");
    }
    LOG(DBG, ("Sent message of size %lu\n", PAXOS_MSG_SIZE(m)));
}

//Creates a new non-blocking UDP multicast sender for the given address/port
//Returns NULL for error
udp_send_buffer * udp_sendbuf_new(char* address_string, int port) {
    
    //Allocate and clear structure
    udp_send_buffer * sb  = PAX_MALLOC(sizeof(udp_send_buffer));
    memset(sb, 0, sizeof(udp_send_buffer));

    struct sockaddr_in * addr_p = &sb->addr;
        
    // Set up socket 
    sb->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sb->sock < 0) {
        perror("sender socket");
        return NULL;
    }
    
    // Set up address 
    memset(addr_p, '\0', sizeof(struct sockaddr_in));
    addr_p->sin_addr.s_addr = inet_addr(address_string);
    if (addr_p->sin_addr.s_addr == INADDR_NONE) {
        printf("Error setting addr\n");
        return NULL;
    }
    addr_p->sin_family = AF_INET;
    addr_p->sin_port = htons((uint16_t)port);	
    // addrlen = sizeof(struct sockaddr_in);
    
#ifdef PAXOS_UDP_SEND_NONBLOCK
    // Set non-blocking 
    int flag = fcntl(sb->sock, F_GETFL);
    if(flag < 0) {
        perror("fcntl1");
        return NULL;
    }
    flag |= O_NONBLOCK;
    if(fcntl(sb->sock, F_SETFL, flag) < 0) {
        perror("fcntl2");
        return NULL;
    }
#endif

    LOG(DBG, ("Socket %d created for address %s:%d (send mode)\n", sb->sock, address_string, port));
    return sb;
}
    

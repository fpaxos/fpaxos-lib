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


#ifndef _LIBPAXOS_MESSAGES_H_
#define _LIBPAXOS_MESSAGES_H_

#include "paxos.h"
#include <stdlib.h>

/*
    Paxos message types
*/

typedef enum pax_msg_code_e {
	prepare_reqs=1,     //Phase 1a, P->A
	prepare_acks=2,     //Phase 1b, A->P
	accept_reqs=4,      //Phase 2a, P->A
	accept_acks=8,      //Phase 2b, A->L
	repeat_reqs=16,     //For progress, L -> A
	submit=32,          //Clients to leader
	leader_announce=64, //Oracle to proposers
	alive_ping=65       //Proposers to oracle
} paxos_msg_code;

typedef struct paxos_msg_t {
	size_t data_size; //Size of 'data' in bytes
	paxos_msg_code type;
	char data[0];
}  __attribute__((packed)) paxos_msg;
#define PAXOS_MSG_SIZE(M) (M->data_size + sizeof(paxos_msg))

/* 
	Paxos protocol messages
*/
//Phase 1a, prepare request
typedef struct prepare_req_t {
	iid_t iid;
	ballot_t ballot;
} prepare_req;
#define PREPARE_REQ_SIZE(M) (sizeof(prepare_req))

//Phase 1b, prepare acknowledgement
typedef struct prepare_ack_t {
	int acceptor_id;
	iid_t iid;
	ballot_t ballot;
	ballot_t value_ballot;
	size_t value_size;
	char value[0];
} prepare_ack;
#define PREPARE_ACK_SIZE(M) (M->value_size + sizeof(prepare_ack)) 

//Phase 2a, accept request
typedef struct accept_req_t {
	iid_t iid;
	ballot_t ballot;
	size_t value_size;
	char value[0];
} accept_req;
#define ACCEPT_REQ_SIZE(M) (M->value_size + sizeof(accept_req))

//Phase 2b, accept acknowledgement
typedef struct accept_ack_t {
	int acceptor_id;
	iid_t iid;
	ballot_t ballot;
	ballot_t value_ballot;
	short int is_final;
	size_t value_size;
	char value[0];
} accept_ack;
#define ACCEPT_ACK_SIZE(M) (M->value_size + sizeof(accept_ack))

//Storage type
typedef accept_ack acceptor_record;
#define ACCEPT_RECORD_BUFF_SIZE(value_size) (value_size + sizeof(accept_ack))

#endif

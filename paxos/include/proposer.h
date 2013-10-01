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


#ifndef _PROPOSER_H_
#define _PROPOSER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "paxos.h"
#include "libpaxos_messages.h"

struct proposer;
struct timeout_iterator;

struct proposer* proposer_new(int id, int acceptors);
void proposer_free(struct proposer* p);
void proposer_propose(struct proposer* p, const char* value, size_t size);
int proposer_prepared_count(struct proposer* p);

// phase 1
void proposer_prepare(struct proposer* p, prepare_req* out);
int proposer_receive_prepare_ack(struct proposer* p, prepare_ack* ack, prepare_req* out);

// phase 2
accept_req* proposer_accept(struct proposer* p);
int proposer_receive_accept_ack(struct proposer* p, accept_ack* ack, prepare_req* out);

// timeouts
struct timeout_iterator* proposer_timeout_iterator(struct proposer* p);
prepare_req* timeout_iterator_prepare(struct timeout_iterator* iter);
accept_req* timeout_iterator_accept(struct timeout_iterator* iter);
void timeout_iterator_free(struct timeout_iterator* iter);

#ifdef __cplusplus
}
#endif

#endif

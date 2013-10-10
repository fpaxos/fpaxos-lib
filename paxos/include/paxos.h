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


#ifndef _LIBPAXOS_H_
#define _LIBPAXOS_H_

#include <stdarg.h>
#include <sys/types.h>

/* The maximum message size that paxos will accept */
#define PAXOS_MAX_VALUE_SIZE (256*1000)

/* Paxos types */
typedef u_int32_t iid_t;
typedef u_int32_t ballot_t;

typedef struct {
	u_int paxos_value_len;
	char *paxos_value_val;
} paxos_value;

struct paxos_value_ballot {
	ballot_t ballot;
	paxos_value value;
};
typedef struct paxos_value_ballot paxos_value_ballot;

struct paxos_prepare {
	iid_t iid;
	ballot_t ballot;
};
typedef struct paxos_prepare paxos_prepare;

struct paxos_promise {
	iid_t iid;
	ballot_t ballot;
	paxos_value_ballot *value;
};
typedef struct paxos_promise paxos_promise;

struct paxos_accept {
	iid_t iid;
	ballot_t ballot;
	paxos_value value;
};
typedef struct paxos_accept paxos_accept;

struct paxos_accepted {
	iid_t iid;
	ballot_t ballot;
	ballot_t value_ballot;
	paxos_value value;
	int16_t is_final;
};
typedef struct paxos_accepted paxos_accepted;

struct paxos_repeat {
	iid_t from;
	iid_t to;
};
typedef struct paxos_repeat paxos_repeat;

struct paxos_client_value {
	paxos_value value;
};
typedef struct paxos_client_value paxos_client_value;

enum paxos_message_type {
	PAXOS_PREPARE = 0,
	PAXOS_PROMISE = 1,
	PAXOS_ACCEPT = 2,
	PAXOS_ACCEPTED = 3,
	PAXOS_REPEAT = 4,
	PAXOS_CLIENT_VALUE = 5,
};
typedef enum paxos_message_type paxos_message_type;

struct paxos_message {
	paxos_message_type type;
	union {
		struct paxos_prepare prepare;
		struct paxos_promise promise;
		struct paxos_accept accept;
		struct paxos_accepted accepted;
		struct paxos_repeat repeat;
		struct paxos_client_value client_value;
	} paxos_message_u;
};
typedef struct paxos_message paxos_message;


/* Configuration */
struct paxos_config
{ 
	int verbosity;
	
	/* Learner */
	int learner_instances;
	int learner_catch_up;
	
	/* Proposer */
	int proposer_timeout;
	int proposer_preexec_window;
	
	/* Acceptor */
	
	/* BDB storage configuration */
	int bdb_sync;
	int bdb_cachesize;
	char* bdb_env_path;
	char* bdb_db_filename;
	int bdb_trash_files;
};

extern struct paxos_config paxos_config;

/* Logging and verbosity levels */
#define PAXOS_LOG_QUIET 0
#define PAXOS_LOG_ERROR 1
#define PAXOS_LOG_INFO 2
#define PAXOS_LOG_DEBUG 3

/* Core functions */
int paxos_quorum(int acceptors);
paxos_value* paxos_value_new(const char* v, size_t s);
void paxos_value_free(paxos_value* v);
void paxos_promise_destroy(paxos_promise* p);
void paxos_accepted_destroy(paxos_accepted* a);
void paxos_accepted_free(paxos_accepted* a);
void paxos_log(int level, const char* format, va_list ap);
void paxos_log_error(const char* format, ...);
void paxos_log_info(const char* format, ...);
void paxos_log_debug(const char* format, ...);

/*
	TODO MAX_N_OF_PROPOSERS should be removed.
	The maximum number of proposers must be fixed beforehand
	(this is because of unique ballot generation).
	The proposers must be started with different IDs.
	This number MUST be a power of 10.
*/
#define MAX_N_OF_PROPOSERS 10

#endif

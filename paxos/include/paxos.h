/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
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

struct paxos_value {
	struct {
		u_int value_len;
		char *value_val;
	} value;
};
typedef struct paxos_value paxos_value;

struct paxos_prepare {
	u_int32_t iid;
	u_int32_t ballot;
};
typedef struct paxos_prepare paxos_prepare;

struct paxos_promise {
	u_int16_t acceptor_id;
	u_int32_t iid;
	u_int32_t ballot;
	u_int32_t value_ballot;
	struct {
		u_int value_len;
		char *value_val;
	} value;
};
typedef struct paxos_promise paxos_promise;

struct paxos_accept {
	u_int32_t iid;
	u_int32_t ballot;
	struct {
		u_int value_len;
		char *value_val;
	} value;
};
typedef struct paxos_accept paxos_accept;

struct paxos_accepted {
	u_int16_t acceptor_id;
	u_int32_t iid;
	u_int32_t ballot;
	u_int32_t value_ballot;
	u_int16_t is_final;
	struct {
		u_int value_len;
		char *value_val;
	} value;
};
typedef struct paxos_accepted paxos_accepted;

struct paxos_repeat {
	u_int32_t from;
	u_int32_t to;
};
typedef struct paxos_repeat paxos_repeat;

struct paxos_client_value {
	struct {
		u_int value_len;
		char *value_val;
	} value;
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


typedef paxos_accepted acceptor_record;

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
paxos_value* paxos_value_dup(const paxos_value* v);
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

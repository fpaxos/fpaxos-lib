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
#include <stdint.h>
#include <sys/types.h>

/* Logging and verbosity levels */
#define PAXOS_LOG_QUIET 0
#define PAXOS_LOG_ERROR 1
#define PAXOS_LOG_INFO 2
#define PAXOS_LOG_DEBUG 3

/* Paxos instance ids and ballots */
typedef uint32_t iid_t;
typedef uint32_t ballot_t;

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

/* Core functions */
int paxos_quorum(int acceptors);
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
#define MAX_N_OF_PROPOSERS  10

#endif

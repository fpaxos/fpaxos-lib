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
#include <stdint.h>
#include <sys/types.h>

/* Logging and verbosity levels */
#define PAXOS_LOG_ERROR 1
#define PAXOS_LOG_INFO 2
#define PAXOS_LOG_DEBUG 3

void paxos_log(int level, const char* format, va_list ap);
void paxos_log_error(const char* format, ...);
void paxos_log_info(const char* format, ...);
void paxos_log_debug(const char* format, ...);

/* The maximum messages size that paxos will accept */
#define PAXOS_MAX_VALUE_SIZE (256*1000)

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


/*** SETTINGS TO BE REMOVED, EVENTUALLY... ***/

/* 
    The maximum number of proposers must be fixed beforehand
    (this is because of unique ballot generation).
    The proposers must be started with different IDs.
    This number MUST be a power of 10.
*/
#define MAX_N_OF_PROPOSERS  10

/* 
    The number of acceptors must be fixed beforehand.
    The acceptors must be started with different IDs.
*/
#define N_OF_ACCEPTORS  3

/* 
    Rule for calculating whether the number of accept_ack messages (phase 2b) 
    is sufficient to declare the instance closed and deliver 
    the corresponding value. i.e.:
    Paxos     -> ((int)(N_OF_ACCEPTORS/2))+1;
    FastPaxos -> 1 + (int)((double)(N_OF_ACCEPTORS*2)/3);
*/

#define QUORUM (((int)(N_OF_ACCEPTORS/2))+1)

#endif

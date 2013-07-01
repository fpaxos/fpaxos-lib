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

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>

/* 
    The maximum size that can be submitted by a client.
    Set MAX_UDP_MSG_SIZE in config file to reflect your network MTU.
    Max packet size minus largest header possible
    (should be accept_ack_batch+accept_ack, around 30 bytes)
    FIXME This should be removed, eventually...
*/
#define PAXOS_MAX_VALUE_SIZE (256*1000)


/* 
    Alias for instance identifier and ballot number.
*/
typedef uint32_t ballot_t;
typedef uint32_t iid_t;


struct paxos_config
{ 
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

/*** LOGGING MACROS ***/

#define VRB 1
#define DBG 3

#define LOG(L, S) if(VERBOSITY_LEVEL >= L) {\
	printf("[%s] ", __func__) ;\
	printf S ;\
}

/*** DEBUGGING SETTINGS ***/

/*
  Verbosity of the library
  0 -> off (prints only errors)
  1 -> verbose 
  3 -> debug
*/
#define VERBOSITY_LEVEL 0

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

#endif /* _LIBPAXOS_H_ */

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
#define PAXOS_MAX_VALUE_SIZE (8000)

/* 
    Alias for instance identifier and ballot number.
*/
typedef uint32_t ballot_t;
typedef uint32_t iid_t;

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

#endif /* _LIBPAXOS_H_ */

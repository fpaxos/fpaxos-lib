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


#include "paxos.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>


struct paxos_config paxos_config =
{
	PAXOS_LOG_INFO,    /* verbosity */
	2048,              /* learner_instances */
	1,                 /* learner_catchup */
	1,                 /* proposer_timeout */
	128,               /* proposer_preexec_window */
	0,                 /* bdb_sync */
	32*1024*1024,      /* bdb_cachesize */
	"/tmp/acceptor",   /* bdb_env_path */
	"acc.bdb",         /* bdb_db_filename */
	0,                 /* bdb_delete_on_restart */
};


void
paxos_log(int level, const char* format, va_list ap)
{
	int off;
	char msg[1024];
	struct timeval tv;
	
	if (level > paxos_config.verbosity)
		return;
	
	gettimeofday(&tv,NULL);
	off = strftime(msg, sizeof(msg), "%d %b %H:%M:%S. ", localtime(&tv.tv_sec));
	vsnprintf(msg+off, sizeof(msg)-off, format, ap);
	fprintf(stdout,"%s\n", msg);
}

void
paxos_log_error(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	paxos_log(PAXOS_LOG_ERROR, format, ap);
	va_end(ap);
}

void
paxos_log_info(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	paxos_log(PAXOS_LOG_INFO, format, ap);
	va_end(ap);
}

void
paxos_log_debug(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	paxos_log(PAXOS_LOG_DEBUG, format, ap);
	va_end(ap);
}

int
paxos_quorum(int acceptors)
{
	return (acceptors/2)+1;
}

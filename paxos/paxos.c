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

/*
 * Copyright (c) 2013-2015, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "paxos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

struct paxos_config paxos_config =
{
	.verbosity = PAXOS_LOG_INFO,
	.tcp_nodelay = 1,
	.learner_catch_up = 1,
	.proposer_timeout = 1,
	.proposer_preexec_window = 128,
	.storage_backend = PAXOS_MEM_STORAGE,
	.trash_files = 0,
	.lmdb_sync = 0,
	.lmdb_env_path = "/tmp/acceptor",
	.lmdb_mapsize = 10*1024*1024
};


int
paxos_quorum(int acceptors)
{
	return (acceptors/2)+1;
}

paxos_value*
paxos_value_new(const char* value, size_t size)
{
	paxos_value* v;
	v = malloc(sizeof(paxos_value));
	v->paxos_value_len = size;
	v->paxos_value_val = malloc(size);
	memcpy(v->paxos_value_val, value, size);
	return v;
}

void
paxos_value_free(paxos_value* v)
{
	free(v->paxos_value_val);
	free(v);
}

static void
paxos_value_destroy(paxos_value* v)
{
	if (v->paxos_value_len > 0)
		free(v->paxos_value_val);	
}

void
paxos_accepted_free(paxos_accepted* a)
{
	paxos_accepted_destroy(a);
	free(a);
}

void
paxos_promise_destroy(paxos_promise* p)
{
	paxos_value_destroy(&p->value);
}

void
paxos_accept_destroy(paxos_accept* p)
{
	paxos_value_destroy(&p->value);
}

void
paxos_accepted_destroy(paxos_accepted* p)
{
	paxos_value_destroy(&p->value);
}

void
paxos_client_value_destroy(paxos_client_value* p)
{
	paxos_value_destroy(&p->value);
}

void
paxos_message_destroy(paxos_message* m)
{
	switch (m->type) {
	case PAXOS_PROMISE:
		paxos_promise_destroy(&m->u.promise);
		break;
	case PAXOS_ACCEPT:
		paxos_accept_destroy(&m->u.accept);
		break;
	case PAXOS_ACCEPTED:
		paxos_accepted_destroy(&m->u.accepted);
		break;
	case PAXOS_CLIENT_VALUE:
		paxos_client_value_destroy(&m->u.client_value);
		break;
	default: break;
	}
}

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

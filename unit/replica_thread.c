/*
 * Copyright (c) 2015, University of Lugano
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


#include <evpaxos.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "replica_thread.h"


static void
replica_thread_deliver(unsigned iid, char* value, size_t size, void* arg)
{
	struct replica_thread* self = arg;
	assert(size == sizeof(int));
	self->delivery_values[iid-1] = *(int*)value;
	if (iid == self->delivery_count)
		pthread_exit(NULL);
}

static void*
replica_thread_run(void* arg)
{
	struct replica_thread* self = arg;
	event_base_dispatch(self->base);
	return NULL;
}

void
replica_thread_create(struct replica_thread* self, int id, const char* config, 
	int delivery_count)
{
	self->delivery_count = delivery_count;
	self->delivery_values = calloc(delivery_count, sizeof(int));
	self->base = event_base_new();
	self->replica = evpaxos_replica_init(id, config, replica_thread_deliver,
		self, self->base);
	pthread_create(&self->thread, NULL, replica_thread_run, self);
}

void
replica_thread_stop(struct replica_thread* self)
{
	pthread_cancel(self->thread);
}

int*
replica_thread_wait_deliveries(struct replica_thread* self)
{
	pthread_join(self->thread, NULL);
	return self->delivery_values;
}

void
replica_thread_destroy(struct replica_thread* self)
{
	free(self->delivery_values);
	evpaxos_replica_free(self->replica);
	event_base_free(self->base);
}

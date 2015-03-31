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


#include "gtest/gtest.h"
#include "evpaxos.h"
#include "test_client.h"
#include "replica_thread.h"


int start_replicas_from_config(const char* config_file, 
	struct replica_thread** threads, int deliveries)
{
	int i;
	struct evpaxos_config* config = evpaxos_config_read(config_file);
	int count = evpaxos_acceptor_count(config);
	*threads = (replica_thread*)calloc(count, sizeof(struct replica_thread));
	for (i = 0; i < count; i++)
		replica_thread_create(&(*threads)[i], i, config_file, deliveries);
	evpaxos_config_free(config);
	return count;
}

TEST(ReplicaTest, TotalOrderDelivery) {
	struct replica_thread* threads;
	int i, j, replicas, deliveries = 100000;
		
	replicas = start_replicas_from_config("config/replicas.conf", 
		&threads, deliveries);
	test_client* client = test_client_new("config/replicas.conf", 0);

	for (i = 0; i < deliveries; i++)
		test_client_submit_value(client, i);
	
	int* values[replicas];
	for (i = 0; i < replicas; i++)
		values[i] = replica_thread_wait_deliveries(&threads[i]);
	
	for (i = 0; i < replicas; i++)
		for (j = 0; j < deliveries; j++)
			ASSERT_EQ(values[i][j], j);
	
	test_client_free(client);
	for (i = 0; i < replicas; i++)
		replica_thread_destroy(&threads[i]);
}

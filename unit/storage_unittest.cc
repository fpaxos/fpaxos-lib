/*
 * Copyright (c) 2014, University of Lugano
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

#include "storage.h"
#include "gtest/gtest.h"

class StorageTest : public::testing::TestWithParam<paxos_storage_backend> {
protected:

	struct storage store;
	
	virtual void SetUp() {
		paxos_config.verbosity = PAXOS_LOG_ERROR;
		paxos_config.storage_backend = GetParam();
		paxos_config.trash_files = 1;
		storage_init(&store, 0);
		storage_open(&store);
	}
	
	virtual void TearDown() {
		storage_close(&store);
	}
	
	void TestPutManyInstances(iid_t from, iid_t to) {
		paxos_accepted accepted = {0, 0, 0, 0, {0, NULL}};
		storage_tx_begin(&store);
		for (int i = from; i <= to; i++) {
			accepted.iid = i;
			storage_put_record(&store, &accepted);
		}
		storage_tx_commit(&store);
	}
	
	void TestCheckInstancesDeleted(iid_t from, iid_t to) {
		paxos_accepted accepted;
		storage_tx_begin(&store);
		for (int i = from; i <= to; ++i) {
			int rv = storage_get_record(&store, i, &accepted);
			ASSERT_EQ(rv, 0);
		}
		storage_tx_commit(&store);
	}
	
	void TestCheckInstancesExist(iid_t from, iid_t to) {
		paxos_accepted accepted;
		storage_tx_begin(&store);
		for (int i = from; i <= to; ++i) {
			int rv = storage_get_record(&store, i, &accepted);
			ASSERT_EQ(rv, 1);
			paxos_accepted_destroy(&accepted);
		}
		storage_tx_commit(&store);
	}
};

TEST_P(StorageTest, GetInitialTrimInstance) {
	storage_tx_begin(&store);
	iid_t iid = storage_get_trim_instance(&store);
	storage_tx_commit(&store);
	ASSERT_EQ(iid, 0);
}

TEST_P(StorageTest, TrimInstanceZero) {
	paxos_accepted accepted = {0, 1, 0, 0, {0, NULL}};

	storage_tx_begin(&store);
	storage_put_record(&store, &accepted);
	storage_tx_commit(&store);

	storage_tx_begin(&store);
	storage_trim(&store, 0);
	storage_tx_commit(&store);

	storage_tx_begin(&store);
	iid_t iid = storage_get_trim_instance(&store);
	storage_tx_commit(&store);
	ASSERT_EQ(iid, 0);
}

TEST_P(StorageTest, TrimAccepted) {
	iid_t iid;
	
	TestPutManyInstances(1, 1000);

	storage_tx_begin(&store);
	storage_trim(&store, 500);
	storage_tx_commit(&store);

	storage_tx_begin(&store);
	iid = storage_get_trim_instance(&store);
	storage_tx_commit(&store);
	ASSERT_EQ(iid, 500);

	TestCheckInstancesDeleted(1, 500);
	TestCheckInstancesExist(501, 1000);

	TestPutManyInstances(1000, 2000);

	storage_tx_begin(&store);
	storage_trim(&store, 1500);
	storage_tx_commit(&store);

	storage_tx_begin(&store);
	iid = storage_get_trim_instance(&store);
	storage_tx_commit(&store);
	ASSERT_EQ(iid, 1500);

	TestCheckInstancesDeleted(1000, 1500);
	TestCheckInstancesExist(1501, 2000);
}

TEST_P(StorageTest, TrimWithHoles) {
	TestPutManyInstances(10, 200);
	TestPutManyInstances(200, 300);
	TestPutManyInstances(400, 600);

	storage_tx_begin(&store);
	storage_trim(&store, 500);
	storage_tx_commit(&store);

	storage_tx_begin(&store);
	iid_t iid = storage_get_trim_instance(&store);
	storage_tx_commit(&store);
	ASSERT_EQ(iid, 500);

	TestCheckInstancesDeleted(1, 500);
	TestCheckInstancesExist(501, 600);
}

paxos_storage_backend backends[] = {
	PAXOS_MEM_STORAGE,
#if HAS_LMDB
	PAXOS_LMDB_STORAGE,
#endif
};

INSTANTIATE_TEST_CASE_P(StorageBackends, StorageTest, 
	testing::ValuesIn(backends));

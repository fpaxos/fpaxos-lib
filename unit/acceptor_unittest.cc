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


#include "acceptor.h"
#include <gtest/gtest.h>

class AcceptorTest : public testing::Test {
protected:

	int id;
	struct acceptor* a;
	
	virtual void SetUp() {
		id = 2;
		paxos_config.bdb_trash_files = 1;
		paxos_config.verbosity = PAXOS_LOG_QUIET;
		a = acceptor_new(id);
	}
	
	virtual void TearDown() {
		acceptor_free(a);
	}
};

TEST_F(AcceptorTest, Prepare) {
	paxos_prepare pre = {1, 101};
	paxos_promise pro;
	acceptor_receive_prepare(a, &pre, &pro);
	ASSERT_EQ(pro.acceptor_id, id);
	ASSERT_EQ(pro.iid, 1);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(pro.value_ballot, 0);
	ASSERT_EQ(NULL, pro.value.value_val);
}

TEST_F(AcceptorTest, PrepareDuplicate) {
	paxos_prepare pre = {1, 101};
	paxos_promise pro;
	acceptor_receive_prepare(a, &pre, &pro);
	acceptor_receive_prepare(a, &pre, &pro);
	ASSERT_EQ(pro.acceptor_id, id);
	ASSERT_EQ(pro.iid, 1);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(pro.value_ballot, 0);
	ASSERT_EQ(NULL, pro.value.value_val);
}

TEST_F(AcceptorTest, PrepareSmallerBallot) {
	paxos_prepare pre;
	paxos_promise pro;
	int ballots[] = {11, 5, 9, 10, 2};
	for (int i = 0; i < (sizeof(ballots)/sizeof(int)); ++i) {
		pre = (paxos_prepare) {1, ballots[i]};
		acceptor_receive_prepare(a, &pre, &pro);
		ASSERT_EQ(pro.ballot, ballots[0]);
	}
}

TEST_F(AcceptorTest, PrepareHigherBallot) {
	paxos_prepare pre;
	paxos_promise pro;
	int ballots[] = {0, 10, 11, 20, 33};
	for (int i = 0; i < sizeof(ballots)/sizeof(int); ++i) {
		pre = (paxos_prepare) {1, ballots[i]};
		acceptor_receive_prepare(a, &pre, &pro);
		ASSERT_EQ(pro.ballot, ballots[i]);
	}
}

TEST_F(AcceptorTest, Accept) {
	paxos_accept ar = {1, 101, 0};	// no value
	paxos_accepted acc;
	acceptor_receive_accept(a, &ar, &acc);
	ASSERT_EQ(acc.acceptor_id, id);
	ASSERT_EQ(acc.iid, 1);
	ASSERT_EQ(acc.ballot, 101);
	ASSERT_EQ(acc.is_final, 0);
	ASSERT_EQ(acc.value_ballot, 101);
	ASSERT_EQ(NULL, acc.value.value_val);
}

TEST_F(AcceptorTest, AcceptPrepared) {
	paxos_prepare pre = {1, 101};
	paxos_accept accept = {1, 101, 0};
	paxos_promise pro;
	paxos_accepted accepted;
	
	acceptor_receive_prepare(a, &pre, &pro);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(pro.value_ballot, 0);
	
	acceptor_receive_accept(a, &accept, &accepted);
	ASSERT_EQ(accepted.ballot, 101);
	ASSERT_EQ(accepted.value_ballot, 101);
}

TEST_F(AcceptorTest, AcceptHigherBallot) {
	paxos_prepare pr = {1, 101};
	paxos_accept ar = {1, 201, 0};
	paxos_promise pro;
	paxos_accepted acc;
	
	acceptor_receive_prepare(a, &pr, &pro);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(pro.value_ballot, 0);
	
	acceptor_receive_accept(a, &ar, &acc);
	ASSERT_EQ(acc.ballot, 201);
	ASSERT_EQ(acc.value_ballot, 201);
}

TEST_F(AcceptorTest, AcceptSmallerBallot) {
	paxos_prepare pr = {1, 201};
	paxos_accept ar = {1, 101, 0};
	paxos_promise pro;
	paxos_accepted acc;
	
	acceptor_receive_prepare(a, &pr, &pro);
	ASSERT_EQ(pro.ballot, 201);
	ASSERT_EQ(pro.value_ballot, 0);
	
	acceptor_receive_accept(a, &ar, &acc);
	ASSERT_EQ(acc.ballot, 201);
	ASSERT_EQ(acc.value_ballot, 0);
}

TEST_F(AcceptorTest, PrepareWithAcceptedValue) {
	paxos_prepare pr = {1, 101};
	paxos_accept ar = {1, 101, 0};
	paxos_promise pro;
	paxos_accepted acc;
	
	acceptor_receive_prepare(a, &pr, &pro);
	acceptor_receive_accept(a, &ar, &acc);
	
	pr = (paxos_prepare) {1, 201};
	acceptor_receive_prepare(a, &pr, &pro);
	ASSERT_EQ(pro.ballot, 201);
	ASSERT_EQ(pro.value_ballot, 101);
}

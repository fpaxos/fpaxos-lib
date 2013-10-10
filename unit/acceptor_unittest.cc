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
	ASSERT_EQ(pro.iid, 1);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(NULL, pro.value);
}

TEST_F(AcceptorTest, PrepareDuplicate) {
	paxos_prepare pre = {1, 101};
	paxos_promise pro;
	acceptor_receive_prepare(a, &pre, &pro);
	acceptor_receive_prepare(a, &pre, &pro);
	ASSERT_EQ(pro.iid, 1);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(NULL, pro.value);
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
	paxos_accept ar = {1, 101, {4, (char*)"foo"}};
	paxos_accepted acc;
	acceptor_receive_accept(a, &ar, &acc);
	ASSERT_EQ(acc.iid, 1);
	ASSERT_EQ(acc.ballot, 101);
	ASSERT_EQ(acc.is_final, 0);
	ASSERT_EQ(acc.value_ballot, 101);
	ASSERT_STREQ("foo", acc.value.paxos_value_val);
	paxos_accepted_destroy(&acc);
}

TEST_F(AcceptorTest, AcceptPrepared) {
	paxos_prepare pr = {1, 101};
	paxos_accept ar = {1, 101, {8 , (char*)"foo bar"}};
	paxos_promise pro;
	paxos_accepted acc;
	
	acceptor_receive_prepare(a, &pr, &pro);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(NULL, pro.value);
	
	acceptor_receive_accept(a, &ar, &acc);
	ASSERT_EQ(acc.ballot, 101);
	ASSERT_EQ(acc.value_ballot, 101);
	
	paxos_accepted_destroy(&acc);
}

TEST_F(AcceptorTest, AcceptHigherBallot) {
	paxos_prepare pr = {1, 101};
	paxos_accept ar = {1, 201, {4, (char*)"baz"}};
	paxos_promise pro;
	paxos_accepted acc;
	
	acceptor_receive_prepare(a, &pr, &pro);
	ASSERT_EQ(pro.ballot, 101);
	ASSERT_EQ(NULL, pro.value);
	
	acceptor_receive_accept(a, &ar, &acc);
	ASSERT_EQ(acc.ballot, 201);
	ASSERT_EQ(acc.value_ballot, 201);
	paxos_accepted_destroy(&acc);
}

TEST_F(AcceptorTest, AcceptSmallerBallot) {
	paxos_prepare pr = {1, 201};
	paxos_accept ar = {1, 101, {4, (char*)"bar"}};
	paxos_promise pro;
	paxos_accepted acc;
	
	acceptor_receive_prepare(a, &pr, &pro);
	ASSERT_EQ(pro.ballot, 201);
	ASSERT_EQ(NULL, pro.value);
	
	acceptor_receive_accept(a, &ar, &acc);
	ASSERT_EQ(acc.ballot, 201);
	ASSERT_EQ(acc.value_ballot, 0);
}

TEST_F(AcceptorTest, PrepareWithAcceptedValue) {
	paxos_prepare pr = {1, 101};
	paxos_accept ar = {1, 101, {4, (char*)"bar"}};
	paxos_promise pro;
	paxos_accepted acc;
	
	acceptor_receive_prepare(a, &pr, &pro);
	acceptor_receive_accept(a, &ar, &acc);
	paxos_accepted_destroy(&acc);
	
	pr = (paxos_prepare) {1, 201};
	acceptor_receive_prepare(a, &pr, &pro);
	ASSERT_EQ(pro.ballot, 201);
	ASSERT_EQ(pro.value->ballot, 101);
	paxos_promise_destroy(&pro);
}

TEST_F(AcceptorTest, Repeat) {
	int found;
	paxos_accept ar = {10, 101, {10, (char*)"aaaaaaaaa"}};
	paxos_accepted acc;
	
	acceptor_receive_accept(a, &ar, &acc);
	paxos_accepted_destroy(&acc);
	ASSERT_FALSE(acceptor_receive_repeat(a, 1, &acc));
	paxos_accepted_destroy(&acc);
	ASSERT_TRUE(acceptor_receive_repeat(a, 10, &acc));
	paxos_accepted_destroy(&acc);
}

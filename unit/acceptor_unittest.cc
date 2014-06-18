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
	prepare_req p = {1, 101};
	acceptor_record* rec = acceptor_receive_prepare(a, &p);
	ASSERT_EQ(rec->acceptor_id, id);
	ASSERT_EQ(rec->iid, 1);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->is_final, 0);
	ASSERT_EQ(rec->value_ballot, 0);
	ASSERT_EQ(rec->value_size, 0);
	acceptor_free_record(a, rec);
}

TEST_F(AcceptorTest, PrepareDuplicate) {

	prepare_req p = {1, 101};
	acceptor_record* rec = acceptor_receive_prepare(a, &p);
	acceptor_record* dup = acceptor_receive_prepare(a, &p);
	ASSERT_EQ(dup->acceptor_id, id);
	ASSERT_EQ(dup->iid, 1);
	ASSERT_EQ(dup->ballot, 101);
	ASSERT_EQ(dup->is_final, 0);
	ASSERT_EQ(dup->value_ballot, 0);
	ASSERT_EQ(dup->value_size, 0);
	acceptor_free_record(a, dup);	
	acceptor_free_record(a, rec);	
}

TEST_F(AcceptorTest, PrepareSmallerBallot) {
	prepare_req p;
	acceptor_record* rec;
	int ballots[] = {11, 5, 9};
	
	for (int i = 0; i < (sizeof(ballots)/sizeof(int)); ++i) {
		p = (prepare_req) {1, ballots[i]};
		rec = acceptor_receive_prepare(a, &p);
		ASSERT_EQ(rec->ballot, ballots[0]);
		acceptor_free_record(a, rec);
	}
}

TEST_F(AcceptorTest, PrepareHigherBallot) {
	prepare_req p;
	acceptor_record* rec;
	int ballots[] = {0, 10, 11};
	
	for (int i = 0; i < sizeof(ballots)/sizeof(int); ++i) {
		p = (prepare_req) {1, ballots[i]};
		rec = acceptor_receive_prepare(a, &p);
		ASSERT_EQ(rec->ballot, ballots[i]);
		acceptor_free_record(a, rec);
	}
}

TEST_F(AcceptorTest, Accept) {
	acceptor_record* rec;
	accept_req ar = {1, 101, 0};	// no value
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec->acceptor_id, id);
	ASSERT_EQ(rec->iid, 1);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->is_final, 0);
	ASSERT_EQ(rec->value_ballot, 101);
	ASSERT_EQ(rec->value_size, 0);
	acceptor_free_record(a, rec);
}

TEST_F(AcceptorTest, AcceptPrepared) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 101, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 0);
	acceptor_free_record(a, rec);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 101);
	acceptor_free_record(a, rec);

}

TEST_F(AcceptorTest, AcceptHigherBallot) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 201, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 0);
	acceptor_free_record(a, rec);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 201);
	acceptor_free_record(a, rec);

}

TEST_F(AcceptorTest, AcceptSmallerBallot) {
	acceptor_record* rec;
	prepare_req pr = {1, 201};
	accept_req ar = {1, 101, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 0);
	acceptor_free_record(a, rec);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 0);
	acceptor_free_record(a, rec);
}

TEST_F(AcceptorTest, PrepareWithAcceptedValue) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 101, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	acceptor_free_record(a, rec);

	rec = acceptor_receive_accept(a, &ar);
	acceptor_free_record(a, rec);
	
	pr = (prepare_req) {1, 201};
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 101);
	acceptor_free_record(a, rec);

}

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


#include "gtest.h"
#include "acceptor.h"

class AcceptorTest : public testing::Test {
protected:

	int id;
	struct acceptor* a;
        
	virtual void SetUp() {
		id = 2;
		a = acceptor_new(id);
	}
        
	virtual void TearDown() {
		acceptor_delete(a);
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
}

TEST_F(AcceptorTest, PrepareDuplicate) {
	prepare_req p = {1, 101};
	acceptor_receive_prepare(a, &p);
	acceptor_record* rec = acceptor_receive_prepare(a, &p);
	ASSERT_EQ(rec->acceptor_id, id);
	ASSERT_EQ(rec->iid, 1);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->is_final, 0);
	ASSERT_EQ(rec->value_ballot, 0);
	ASSERT_EQ(rec->value_size, 0);
}

TEST_F(AcceptorTest, PrepareSmallerBallot) {
	prepare_req p;
	acceptor_record* rec;
	int ballots[] = {11, 5, 9};
	
	for (int i = 0; i < (sizeof(ballots)/sizeof(int)); ++i) {
		p = (prepare_req) {1, ballots[i]};
		rec = acceptor_receive_prepare(a, &p);
		ASSERT_EQ(rec->ballot, ballots[0]);
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
}

TEST_F(AcceptorTest, AcceptPrepared) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 101, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 0);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 101);
}

TEST_F(AcceptorTest, AcceptHigherBallot) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 201, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 0);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 201);
}

TEST_F(AcceptorTest, AcceptSmallerBallot) {
	acceptor_record* rec;
	prepare_req pr = {1, 201};
	accept_req ar = {1, 101, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 0);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 0);
}

TEST_F(AcceptorTest, PrepareWithAcceptedValue) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 101, 0};
	
	acceptor_receive_prepare(a, &pr);
	acceptor_receive_accept(a, &ar);
	
	pr = (prepare_req) {1, 201};
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 101);
}

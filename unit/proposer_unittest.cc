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


#include "proposer.h"
#include <gtest/gtest.h>

#define CHECK_ACCEPT_REQ(r, i, b, v, s) { \
	ASSERT_NE(r, (void*)NULL);            \
	ASSERT_EQ(r->iid, i);                 \
	ASSERT_EQ(r->ballot, b);              \
	ASSERT_EQ(r->value_size, s);          \
	ASSERT_STREQ(r->value, v);            \
}

class ProposerTest : public testing::Test {
protected:

	int id;
	struct proposer* p;

	virtual void SetUp() {
		id = 2;
		p = proposer_new(id);
	}
	
	virtual void TearDown() {
		proposer_free(p);
	}
};

prepare_ack* prepare_ack_with_value(prepare_ack pa, char* value, size_t size) {
	prepare_ack* ack = (prepare_ack*)malloc(sizeof(prepare_ack) + size);
	*ack = pa;
	ack->value_size = size;
	memcpy(ack->value, value, size);
	return ack;
}

TEST_F(ProposerTest, Prepare) {
	int count = 10;
	prepare_req pr;
	for (int i = 0; i < count; ++i) {
		pr = proposer_prepare(p);
		ASSERT_EQ(pr.iid, i+1);
		ASSERT_EQ(pr.ballot, id + MAX_N_OF_PROPOSERS);
	}
	ASSERT_EQ(count, proposer_prepared_count(p));
}

TEST_F(ProposerTest, PrepareAndAccept) {
	char value[] = "a value";
	int value_size = strlen(value) + 1;	
	prepare_req pr = proposer_prepare(p);

	for (size_t i = 0; i < QUORUM; ++i) {
		prepare_ack pa = (prepare_ack) {i, pr.iid, pr.ballot, 0, 0};
		ASSERT_EQ((void*)NULL, proposer_receive_prepare_ack(p, &pa));
	}
	
	// we have no value to propose!
	ASSERT_EQ((void*)NULL, proposer_accept(p));
	
	proposer_propose(p, value, value_size);
	
	accept_req* ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, value, value_size);
	
	for (size_t i = 0; i < QUORUM; ++i) {
		accept_ack aa = (accept_ack) {i, ar->iid, ar->ballot, ar->ballot, 0, 0};
		ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);
	}
	free(ar);
}

TEST_F(ProposerTest, PreparePreempted) {
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	prepare_req pr = proposer_prepare(p);
	proposer_propose(p, value, value_size);
	
	// preempt! proposer receives a different ballot...
	prepare_ack pa = (prepare_ack) {1, pr.iid, pr.ballot+1, 0, 0};
	prepare_req* pr_preempt = proposer_receive_prepare_ack(p, &pa);
	ASSERT_NE((void*)NULL, pr_preempt);
	ASSERT_EQ(pr_preempt->iid, pr.iid);
	ASSERT_GT(pr_preempt->ballot, pr.ballot);
	
	for (size_t i = 0; i < QUORUM; ++i) {
		pa = (prepare_ack) {i, pr_preempt->iid, pr_preempt->ballot, 0, 0};
		ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	}
	
	accept_req* ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr_preempt->iid, pr_preempt->ballot, value, value_size);
	free(ar);
	free(pr_preempt);
}

TEST_F(ProposerTest, PrepareAlreadyClosed) {
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	prepare_req pr = proposer_prepare(p);
	proposer_propose(p, value, value_size);

	// preempt! proposer receives a different ballot...
	prepare_ack* pa = prepare_ack_with_value(
		(prepare_ack) {1, pr.iid, pr.ballot+1, 0, 0}, (char*)"foo bar baz", 12);
	prepare_req* pr_preempt = proposer_receive_prepare_ack(p, pa);
	ASSERT_NE((void*)NULL, pr_preempt);
	ASSERT_EQ(pr_preempt->iid, pr.iid);
	ASSERT_GT(pr_preempt->ballot, pr.ballot);
	free(pa);

	// acquire the instance
	pa = prepare_ack_with_value(
		(prepare_ack){0, pr_preempt->iid, pr_preempt->ballot, 0, 0},
		(char*)"foo bar baz", 12);
	
	for (size_t i = 0; i < QUORUM; ++i) {
		pa->acceptor_id = i;
		ASSERT_EQ(proposer_receive_prepare_ack(p, pa), (void*)NULL);
	}
	free(pa);
	free(pr_preempt);

	// proposer has majority with the same value, 
	// we expect the instance to be closed
	accept_req* ar = proposer_accept(p);
	ASSERT_EQ(ar, (void*)NULL);

	// try to accept the first value on instance 2
	pr = proposer_prepare(p);
	for (int i = 0; i < QUORUM; ++i) {
		prepare_ack ack = (prepare_ack) {i, pr.iid, pr.ballot, 0, 0};
		ASSERT_EQ(proposer_receive_prepare_ack(p, &ack), (void*)NULL);
	}
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, value, value_size)
	free(ar);
}

TEST_F(ProposerTest, AcceptPreempted) {
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	prepare_req pr = proposer_prepare(p);
	proposer_propose(p, value, value_size);
	
	for (size_t i = 0; i < QUORUM; ++i) {
		prepare_ack pa = (prepare_ack) {i, pr.iid, pr.ballot, 0, 0};
		ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	}

	accept_req* ar = proposer_accept(p);
	ASSERT_NE(ar, (void*)NULL);
	
	// preempt! proposer receives accept nack
	accept_ack aa = (accept_ack) {0, ar->iid, ar->ballot+1, 0, 0, 0};
	prepare_req* pr_preempt = proposer_receive_accept_ack(p, &aa);	
	ASSERT_NE((void*)NULL, pr_preempt);
	ASSERT_EQ(pr_preempt->iid, pr.iid);
	ASSERT_GT(pr_preempt->ballot, ar->ballot);
	free(ar);
	
	// check that proposer pushed the instance back 
	// to the prepare phase
	ASSERT_EQ(proposer_prepared_count(p), 1);
	
	// finally acquire the instance
	for (size_t i = 0; i < QUORUM; ++i) {
		prepare_ack pa = (prepare_ack) {i, pr_preempt->iid, pr_preempt->ballot, 0, 0};
		ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	}

	// accept again
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr_preempt->iid, pr_preempt->ballot, value, value_size);
	
	for (size_t i = 0; i < QUORUM; ++i) {
		aa = (accept_ack) {i, ar->iid, ar->ballot, ar->ballot, 0, value_size};
		ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);
	}
	free(ar);
	free(pr_preempt);
}

TEST_F(ProposerTest, PreparedCount) {
	int count = 10;
	prepare_req pr;
	char value[] = "a value";
	int value_size = strlen(value) + 1;
	
	for (size_t i = 0; i < count; ++i) {
		pr = proposer_prepare(p);
		proposer_propose(p, value, value_size);
		ASSERT_EQ(i + 1, proposer_prepared_count(p));
	}
	
	for (size_t i = 0; i < count; ++i)
		proposer_accept(p);
	ASSERT_EQ(count, proposer_prepared_count(p));
	
	for (size_t i = 0; i < count; ++i) {
		prepare_ack pa = (prepare_ack) {0, i+1, pr.ballot, 0, 0};
		proposer_receive_prepare_ack(p, &pa);
		pa = (prepare_ack) {1, i+1, pr.ballot, 0, 0};
		proposer_receive_prepare_ack(p, &pa);
		accept_req* ar = proposer_accept(p);
		free(ar);
		ASSERT_EQ(count-(i+1), proposer_prepared_count(p));
	}
}

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

prepare_ack* prepare_ack_with_value(prepare_ack pa, const char* value) {
	int len = strlen(value)+1;
	prepare_ack* ack = (prepare_ack*)malloc(sizeof(prepare_ack) + len);
	*ack = pa;
	ack->value_size = len;
	memcpy(ack->value, value, len);
	return ack;
}

class ProposerTest : public testing::Test {
protected:

	int quorum;
	struct proposer* p;
	static const int id = 2;
	static const int acceptors = 3;
	
	virtual void SetUp() {
		quorum = paxos_quorum(acceptors);
		paxos_config.proposer_timeout = 1;
		p = proposer_new(id, acceptors);
		paxos_config.verbosity = PAXOS_LOG_QUIET;
	}
	
	virtual void TearDown() {
		proposer_free(p);
	}
	
	void TestPrepareAckFromQuorum(iid_t iid, ballot_t bal) {
		prepare_req pr;
		for (size_t i = 0; i < quorum; ++i) {
			prepare_ack pa = (prepare_ack) {i, iid, bal, 0, 0};
			ASSERT_EQ(0, proposer_receive_prepare_ack(p, &pa, &pr));
		}
	}
	
	void TestPrepareAckFromQuorum(iid_t iid, ballot_t bal, 
		const char* value, ballot_t vbal = 0) {
		prepare_req pr;
		prepare_ack* pa;
		pa = prepare_ack_with_value((prepare_ack){0, iid, bal, vbal}, value);
		for (size_t i = 0; i < quorum; ++i) {
			pa->acceptor_id = i;
			ASSERT_EQ(0, proposer_receive_prepare_ack(p, pa, &pr));
		}
		free(pa);
	}
	
	void TestAcceptAckFromQuorum(iid_t iid, ballot_t bal) {
		prepare_req pr;
		for (size_t i = 0; i < quorum; ++i) {
			accept_ack aa = (accept_ack) {i, iid, bal, bal, 0, 0};
			ASSERT_EQ(0, proposer_receive_accept_ack(p, &aa, &pr));
		}
	}
};

TEST_F(ProposerTest, Prepare) {
	int count = 10;
	prepare_req pr;
	for (int i = 0; i < count; ++i) {
		proposer_prepare(p, &pr);
		ASSERT_EQ(pr.iid, i+1);
		ASSERT_EQ(pr.ballot, id + MAX_N_OF_PROPOSERS);
	}
	ASSERT_EQ(count, proposer_prepared_count(p));
}

TEST_F(ProposerTest, IgnoreOldBallots) {
	prepare_req pr, preempted;
	prepare_ack pa;

	proposer_prepare(p, &pr);
	
	// ignore smaller ballot
	pa = (prepare_ack) {1, pr.iid, pr.ballot-1, 0, 0};
	ASSERT_EQ(0, proposer_receive_prepare_ack(p, &pa, &preempted));
	
	// preempt
	pa = (prepare_ack) {1, pr.iid, pr.ballot+1, 0, 0};
	ASSERT_EQ(1, proposer_receive_prepare_ack(p, &pa, &preempted));
	
	// again ignore smaller ballot
	pa = (prepare_ack) {1, pr.iid, pr.ballot, 0, 0};
	ASSERT_EQ(0, proposer_receive_prepare_ack(p, &pa, &preempted));
}

TEST_F(ProposerTest, IgnoreDuplicatePrepareAcks) {
	prepare_req pr, preempted;
	proposer_prepare(p, &pr);
	proposer_propose(p, "value", strlen("value")+1);
	for (size_t i = 0; i < 10; ++i) {
		prepare_ack pa = (prepare_ack) {2, pr.iid, pr.ballot, 0, 0};
		ASSERT_EQ(0, proposer_receive_prepare_ack(p, &pa, &preempted));
		accept_req* ar = proposer_accept(p);
		ASSERT_EQ(NULL, ar);
	}
}

TEST_F(ProposerTest, PrepareAndAccept) {
	prepare_req pr;
	accept_req* ar;
	char value[] = "a value";
	int value_size = strlen(value) + 1;	
	
	proposer_prepare(p, &pr);
	TestPrepareAckFromQuorum(pr.iid, pr.ballot);

	ar = proposer_accept(p);
	ASSERT_EQ((void*)NULL, ar); // no value to propose yet
	
	proposer_propose(p, value, value_size);
	
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, value, value_size);
	
	TestAcceptAckFromQuorum(ar->iid, ar->ballot);
	free(ar);
}

TEST_F(ProposerTest, PreparePreempted) {
	prepare_req pr, preempted;
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	proposer_prepare(p, &pr);
	proposer_propose(p, value, value_size);
	
	// preempt! proposer receives a different ballot...
	prepare_ack pa = (prepare_ack) {1, pr.iid, pr.ballot+1, 0, 0};
	ASSERT_EQ(1, proposer_receive_prepare_ack(p, &pa, &preempted));
	ASSERT_EQ(preempted.iid, pr.iid);
	ASSERT_GT(preempted.ballot, pr.ballot);
	
	TestPrepareAckFromQuorum(preempted.iid, preempted.ballot);
	
	accept_req* ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, preempted.iid, preempted.ballot, value, value_size);
	free(ar);
}

TEST_F(ProposerTest, PrepareAlreadyClosed) {
	prepare_req pr, preempted;
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	proposer_prepare(p, &pr);
	proposer_propose(p, value, value_size);

	// preempt! proposer receives a different ballot...
	prepare_ack* pa = prepare_ack_with_value(
		(prepare_ack) {1, pr.iid, pr.ballot+1, 0, 0}, "foo bar baz");
	ASSERT_EQ(1, proposer_receive_prepare_ack(p, pa, &preempted));
	ASSERT_EQ(preempted.iid, pr.iid);
	ASSERT_GT(preempted.ballot, pr.ballot);
	free(pa);

	// acquire the instance
	TestPrepareAckFromQuorum(preempted.iid, preempted.ballot, value);

	// proposer has majority with the same value, 
	// we expect the instance to be closed
	accept_req* ar = proposer_accept(p);
	TestPrepareAckFromQuorum(ar->iid, ar->ballot);
	free(ar);
	
	// try to accept our value on instance 2
	proposer_prepare(p, &pr);
	TestPrepareAckFromQuorum(pr.iid, pr.ballot);
	
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, value, value_size)
	free(ar);
}

TEST_F(ProposerTest, PreparePreemptedWithTwoValues) {
	prepare_req pr, preempted;

	proposer_prepare(p, &pr);
	proposer_propose(p, "v1", 3);
	
	// preempt with value
	prepare_ack* pa1 = prepare_ack_with_value(
		(prepare_ack){1, pr.iid, pr.ballot+1, pr.ballot+1, 0}, "v2");
	prepare_ack* pa2 = prepare_ack_with_value(
		(prepare_ack){2, pr.iid, pr.ballot+11, pr.ballot+11, 0}, "v3");
	proposer_receive_prepare_ack(p, pa1, &preempted);
	proposer_receive_prepare_ack(p, pa2, &preempted);
	
	pa1->ballot = preempted.ballot;
	proposer_receive_prepare_ack(p, pa1, &preempted);
	
	ASSERT_EQ(NULL, proposer_accept(p));
	
	pa2->ballot = preempted.ballot;
	proposer_receive_prepare_ack(p, pa2, &preempted);
	
	accept_req* ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, preempted.iid, preempted.ballot, "v3", 3);
	free(ar);

	proposer_prepare(p, &pr);
	TestPrepareAckFromQuorum(pr.iid, pr.ballot);
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, "v1", 3);
}

TEST_F(ProposerTest, AcceptPreempted) {
	prepare_req pr, preempted;
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	proposer_prepare(p, &pr);
	proposer_propose(p, value, value_size);
	TestPrepareAckFromQuorum(pr.iid, pr.ballot);
	
	accept_req* ar = proposer_accept(p);
	ASSERT_NE(ar, (void*)NULL);
	
	// preempt! proposer receives accept nack
	accept_ack aa = (accept_ack) {0, ar->iid, ar->ballot+1, ar->ballot+1, 0, 0};
	ASSERT_EQ(1, proposer_receive_accept_ack(p, &aa, &preempted));
	ASSERT_EQ(preempted.iid, pr.iid);
	ASSERT_GT(preempted.ballot, ar->ballot);
	free(ar);
	
	// check that proposer pushed the instance back to the prepare phase
	ASSERT_EQ(proposer_prepared_count(p), 1);
	
	// close the instance
	TestPrepareAckFromQuorum(preempted.iid, preempted.ballot, 
		"preempt", aa.value_ballot);
	ar = proposer_accept(p);
	TestAcceptAckFromQuorum(preempted.iid, preempted.ballot);
	free(ar);
	
	// make sure our value did not disappear...
	proposer_prepare(p, &pr);
	TestPrepareAckFromQuorum(pr.iid, pr.ballot);
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, value, value_size);
	free(ar);
}

TEST_F(ProposerTest, PreparedCount) {
	int count = 100;
	prepare_req pr, preempted;
	
	for (size_t i = 0; i < count; ++i) {
		proposer_prepare(p, &pr);
		proposer_propose(p, "some value", strlen("some value")+1);
		ASSERT_EQ(i + 1, proposer_prepared_count(p));
	}
	
	for (size_t i = 0; i < count; ++i)
		proposer_accept(p);
	ASSERT_EQ(count, proposer_prepared_count(p));
	
	for (size_t i = 0; i < count; ++i) {
		TestPrepareAckFromQuorum(i+1, pr.ballot);
		accept_req* ar = proposer_accept(p);
		free(ar);
		ASSERT_EQ(count-(i+1), proposer_prepared_count(p));
	}
}

TEST_F(ProposerTest, PendingPrepareShouldTimeout) {
	prepare_req pr;
	prepare_req* to;
	struct timeout_iterator* iter;
	
	proposer_prepare(p, &pr);
	sleep(paxos_config.proposer_timeout);
	
	iter = proposer_timeout_iterator(p);
	to = timeout_iterator_prepare(iter);
	
	ASSERT_NE((prepare_req*)NULL, to);
	ASSERT_EQ(pr.iid, to->iid);
	ASSERT_EQ(pr.ballot, to->ballot);
	free(to);
	
	ASSERT_EQ(NULL, timeout_iterator_prepare(iter));
	timeout_iterator_free(iter);
}

TEST_F(ProposerTest, PreparedShouldNotTimeout) {
	struct timeout_iterator* iter;
	prepare_req pr1, pr2, preempted;
	prepare_req* to;
	
	proposer_prepare(p, &pr1);
	proposer_prepare(p, &pr2);
	TestPrepareAckFromQuorum(pr1.iid, pr1.ballot);
	
	sleep(paxos_config.proposer_timeout);
	
	iter = proposer_timeout_iterator(p);
	to = timeout_iterator_prepare(iter);
	ASSERT_NE((prepare_req*)NULL, to);
	ASSERT_EQ(pr2.iid, to->iid);
	ASSERT_EQ(pr2.ballot, to->ballot);
	free(to);
	
	ASSERT_EQ(NULL, timeout_iterator_prepare(iter));
	timeout_iterator_free(iter);
}

TEST_F(ProposerTest, PendingAcceptShouldTimeout) {
	prepare_req pr, preempted;
	accept_req* to;
	
	proposer_prepare(p, &pr);
	proposer_propose(p, "a value", strlen("a value")+1);
	TestPrepareAckFromQuorum(pr.iid, pr.ballot);

	accept_req* ar = proposer_accept(p);
	free(ar);
	
	sleep(paxos_config.proposer_timeout);
	
	struct timeout_iterator* iter = proposer_timeout_iterator(p);
	to = timeout_iterator_accept(iter);
	ASSERT_NE((accept_req*)NULL, to);
	ASSERT_EQ(pr.iid, to->iid);
	ASSERT_EQ(pr.ballot, to->ballot);
	free(to);
	
	ASSERT_EQ(NULL, timeout_iterator_accept(iter));
	timeout_iterator_free(iter);
}

TEST_F(ProposerTest, AcceptedShouldNotTimeout) {
	prepare_req pr, preempted;

	// phase 1
	proposer_prepare(p, &pr);
	proposer_propose(p, "value", strlen("value")+1);
	TestPrepareAckFromQuorum(pr.iid, pr.ballot);
	
	// phase 2
	accept_req* ar = proposer_accept(p);
	TestAcceptAckFromQuorum(ar->iid, ar->ballot);
	free(ar);
	
	// this one should timeout
	proposer_prepare(p, &pr);
	
	sleep(paxos_config.proposer_timeout);
	
	struct timeout_iterator* iter = proposer_timeout_iterator(p);
	prepare_req* to = timeout_iterator_prepare(iter);
	ASSERT_NE((prepare_req*)NULL, to);
	ASSERT_EQ(pr.iid, to->iid);
	ASSERT_EQ(pr.ballot, to->ballot);
	free(to);
	
	ASSERT_EQ(NULL, timeout_iterator_prepare(iter));
	ASSERT_EQ(NULL, timeout_iterator_accept(iter));

	timeout_iterator_free(iter);
}

TEST_F(ProposerTest, ShouldNotTimeoutTwice) {
	prepare_req pr;
	struct timeout_iterator* iter;
	
	proposer_prepare(p, &pr);
	sleep(paxos_config.proposer_timeout);
	
	iter = proposer_timeout_iterator(p);
	prepare_req* to = timeout_iterator_prepare(iter);
	ASSERT_NE((prepare_req*)NULL, to);
	free(to);
	timeout_iterator_free(iter);
	
	iter = proposer_timeout_iterator(p);
	ASSERT_EQ(NULL, timeout_iterator_prepare(iter));
	timeout_iterator_free(iter);
}

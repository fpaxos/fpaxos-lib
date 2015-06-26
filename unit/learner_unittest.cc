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


#include "learner.h"
#include "gtest/gtest.h"

class LearnerTest : public testing::Test {
protected:

	struct learner* l;
	static const int acceptors = 3;
	        
	virtual void SetUp() {
		paxos_config.verbosity = PAXOS_LOG_QUIET;
		l = learner_new(acceptors);
	}
	
	virtual void TearDown() {
		learner_free(l);
	}
};

TEST_F(LearnerTest, Learn) {
	int delivered;
	paxos_accepted a, deliver;

	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);

	// iid, bal, val_bal, final, size
	a = (paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);

	a = (paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_TRUE(delivered);
	ASSERT_EQ(deliver.iid, 1);
	ASSERT_EQ(deliver.ballot, 101);
	ASSERT_EQ(deliver.value_ballot, 101);
	ASSERT_EQ(NULL, deliver.value.paxos_value_val);
	paxos_accepted_destroy(&deliver);
}

TEST_F(LearnerTest, LearnInOrder) {
	int delivered;
	paxos_accepted a, deliver;
	
	// iid, bal, val_bal, final, size
	a = (paxos_accepted) {2, 2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a = (paxos_accepted) {1, 2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	
	// instance 2 can't be delivered before 1
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);
	
	a = (paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a = (paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	
	// deliver instance 1 and then instance 2
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_TRUE(delivered);
	ASSERT_EQ(deliver.iid, 1);
	paxos_accepted_destroy(&deliver);
	
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_TRUE(delivered);
	ASSERT_EQ(deliver.iid, 2);
	paxos_accepted_destroy(&deliver);
}

TEST_F(LearnerTest, IgnoreDuplicates) {
	int delivered;
	paxos_accepted a, deliver;

	a =	(paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	learner_receive_accepted(l, &a);
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);
	
	a = (paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_EQ(deliver.iid, 1);
	paxos_accepted_destroy(&deliver);
}

TEST_F(LearnerTest, LearnMajority) {
	int delivered;
	paxos_accepted a, deliver;

	a =	(paxos_accepted) {0, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a = (paxos_accepted) {1, 1, 100, 100, 0, 0};
	learner_receive_accepted(l, &a);
	
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);

	a = (paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_EQ(deliver.iid, 1);
	ASSERT_EQ(deliver.ballot, 101);
	paxos_accepted_destroy(&deliver);
}

TEST_F(LearnerTest, IgnoreOlderBallot) {
	int delivered;
	paxos_accepted a, deliver;

	a =	(paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a = (paxos_accepted) {1, 1, 201, 201, 0, 0};
	learner_receive_accepted(l, &a);
	
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);
	
	a = (paxos_accepted) {2, 1, 201, 201, 0, 0};
	learner_receive_accepted(l, &a);
	
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_EQ(deliver.iid, 1);
	ASSERT_EQ(deliver.ballot, 201);
	paxos_accepted_destroy(&deliver);
}

TEST_F(LearnerTest, NoHoles) {
	iid_t from, to;
	int delivered;
	paxos_accepted a, deliver;
	a =	(paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_EQ(learner_has_holes(l, &from, &to), 0);
	paxos_accepted_destroy(&deliver);
}

TEST_F(LearnerTest, OneHole) {
	int delivered;
	paxos_accepted a, deliver;
	iid_t from, to;
	
	a =	(paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	paxos_accepted_destroy(&deliver);
	
	a =	(paxos_accepted) {1, 3, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 3, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);
	
	ASSERT_EQ(1, learner_has_holes(l, &from, &to));
	ASSERT_EQ(2, from);
	ASSERT_EQ(3, to);
}

TEST_F(LearnerTest, ManyHoles) {
	int delivered;
	paxos_accepted a, deliver;
	iid_t from, to;
	
	a =	(paxos_accepted) {1, 2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);
	
	a =	(paxos_accepted) {1, 100, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 100, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);
	
	ASSERT_EQ(1, learner_has_holes(l, &from, &to));
	ASSERT_EQ(1, from);
	ASSERT_EQ(100, to);
}

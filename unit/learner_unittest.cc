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


#include "learner.h"
#include <gtest/gtest.h>

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
	paxos_accepted a, *delivered;

	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);

	// iid, bal, val_bal, final, size
	a = (paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);

	a = (paxos_accepted) {1, 101, 101, 0, 0};	
	learner_receive_accepted(l, &a, 2);
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered != NULL);
	ASSERT_EQ(delivered->iid, 1);
	ASSERT_EQ(delivered->ballot, 101);
	ASSERT_EQ(delivered->value_ballot, 101);
	ASSERT_EQ(delivered->is_final, 0);
	ASSERT_EQ(NULL, delivered->value.value_val);
	paxos_accepted_free(delivered);
}

TEST_F(LearnerTest, LearnInOrder) {
	paxos_accepted a, *delivered;
	
	// iid, bal, val_bal, final, size
	a = (paxos_accepted) {2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 2);
	learner_receive_accepted(l, &a, 1);
	
	// instance 2 can't be delivered before 1
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);
	
	a = (paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 2);
	
	// deliver instance 1 and then instance 2
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered != NULL);
	ASSERT_EQ(delivered->iid, 1);
	paxos_accepted_free(delivered);
	
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered != NULL);
	ASSERT_EQ(delivered->iid, 2);
	paxos_accepted_free(delivered);
}

TEST_F(LearnerTest, IgnoreDuplicates) {
	paxos_accepted a, *delivered;

	a =	(paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 1);
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);
	
	a = (paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 2);
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered->iid, 1);
	paxos_accepted_free(delivered);
}

TEST_F(LearnerTest, LearnMajority) {
	paxos_accepted a, *delivered;

	a =	(paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 0);
	a = (paxos_accepted) {1, 100, 100, 0, 0};
	learner_receive_accepted(l, &a, 1);
	
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);

	a = (paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 2);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered->iid, 1);
	ASSERT_EQ(delivered->ballot, 101);
	paxos_accepted_free(delivered);
}

TEST_F(LearnerTest, IgnoreOlderBallot) {
	paxos_accepted a, *delivered;

	a =	(paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	a = (paxos_accepted) {1, 201, 201, 0, 0};
	learner_receive_accepted(l, &a, 1);
	
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);
	
	a = (paxos_accepted) {1, 201, 201, 0, 0};
	learner_receive_accepted(l, &a, 2);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered->iid, 1);
	ASSERT_EQ(delivered->ballot, 201);
	paxos_accepted_free(delivered);
}

TEST_F(LearnerTest, NoHoles) {
	iid_t from, to;
	paxos_accepted a, *delivered;
	a =	(paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 2);
	delivered = learner_deliver_next(l);
	ASSERT_EQ(learner_has_holes(l, &from, &to), 0);
	paxos_accepted_free(delivered);
}

TEST_F(LearnerTest, OneHole) {
	paxos_accepted a, *delivered;
	iid_t from, to;
	
	a =	(paxos_accepted) {1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 2);
	delivered = learner_deliver_next(l);
	paxos_accepted_free(delivered);
	
	a =	(paxos_accepted) {3, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 2);
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);
	
	ASSERT_EQ(1, learner_has_holes(l, &from, &to));
	ASSERT_EQ(2, from);
	ASSERT_EQ(3, to);
}

TEST_F(LearnerTest, ManyHoles) {
	paxos_accepted a, *delivered;
	iid_t from, to;
	
	a =	(paxos_accepted) {2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 2);
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);
	
	a =	(paxos_accepted) {100, 101, 101, 0, 0};
	learner_receive_accepted(l, &a, 1);
	learner_receive_accepted(l, &a, 2);
	delivered = learner_deliver_next(l);
	ASSERT_TRUE(delivered == NULL);
	
	ASSERT_EQ(1, learner_has_holes(l, &from, &to));
	ASSERT_EQ(1, from);
	ASSERT_EQ(100, to);
}

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
	int delivered;
	paxos_accepted a, deliver;

	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);

	// aid, iid, bal, val_bal, final, size
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
	ASSERT_EQ(deliver.is_final, 0);
	ASSERT_EQ(NULL, deliver.value.value_val);
}

TEST_F(LearnerTest, LearnInOrder) {
	int delivered;
	paxos_accepted a, deliver;
	
	// aid, iid, bal, val_bal, final, size
	a = (paxos_accepted) {2, 2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {1, 2, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	
	// instance 2 can't be delivered before 1
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_FALSE(delivered);
	
	a = (paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	
	// deliver instance 1 and then instance 2
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_TRUE(delivered);
	ASSERT_EQ(deliver.iid, 1);
	
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_TRUE(delivered);
	ASSERT_EQ(deliver.iid, 2);
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
}

TEST_F(LearnerTest, NoHoles) {
	int delivered;
	paxos_accepted a, deliver;
	iid_t from, to;
	
	a =	(paxos_accepted) {1, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 1, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	delivered = learner_deliver_next(l, &deliver);
	ASSERT_EQ(learner_has_holes(l, &from, &to), 0);
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
	
	a =	(paxos_accepted) {1, 3, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 3, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	learner_deliver_next(l, &deliver);
	
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
	learner_deliver_next(l, &deliver);
	
	a =	(paxos_accepted) {1, 100, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	a =	(paxos_accepted) {2, 100, 101, 101, 0, 0};
	learner_receive_accepted(l, &a);
	learner_deliver_next(l, &deliver);
	
	ASSERT_EQ(1, learner_has_holes(l, &from, &to));
	ASSERT_EQ(1, from);
	ASSERT_EQ(100, to);
}

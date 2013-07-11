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
	accept_ack a;
	accept_ack* delivered;

	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered , (accept_ack*)NULL);

	// aid, iid, bal, val_bal, final, size
	a = (accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered, (accept_ack*)NULL);

	a = (accept_ack) {2, 1, 101, 101, 0, 0};	
	learner_receive_accept(l, &a);
	delivered = learner_deliver_next(l);
	ASSERT_NE(delivered , (accept_ack*)NULL);
	
	ASSERT_EQ(delivered->iid, 1);
	ASSERT_EQ(delivered->ballot, 101);
	ASSERT_EQ(delivered->value_ballot, 101);
	ASSERT_EQ(delivered->is_final, 0);
	ASSERT_EQ(delivered->value_size, 0);
	
	free(delivered);
}

TEST_F(LearnerTest, LearnInOrder) {
	accept_ack a;
	accept_ack* delivered;
	
	// aid, iid, bal, val_bal, final, size
	a = (accept_ack) {2, 2, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {1, 2, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	
	// instance 2 can't be delivered before 1
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered , (accept_ack*)NULL);
	
	a = (accept_ack) {2, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	
	// deliver instance 1 and then instance 2
	delivered = learner_deliver_next(l);
	ASSERT_NE(delivered, (accept_ack*)NULL);
	ASSERT_EQ(delivered->iid, 1);
	free(delivered);
	
	delivered = learner_deliver_next(l);
	ASSERT_NE(delivered, (accept_ack*)NULL);
	ASSERT_EQ(delivered->iid, 2);
	free(delivered);
}

TEST_F(LearnerTest, IgnoreDuplicates) {
	accept_ack a;
	accept_ack* delivered;

	a =	(accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	learner_receive_accept(l, &a);
	learner_receive_accept(l, &a);
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered , (accept_ack*)NULL);
	
	a = (accept_ack) {2, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered->iid, 1);
	free(delivered);
}

TEST_F(LearnerTest, LearnMajority) {
	accept_ack a;
	accept_ack* delivered;

	a =	(accept_ack) {0, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a = (accept_ack) {1, 1, 100, 100, 0, 0};
	learner_receive_accept(l, &a);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered , (accept_ack*)NULL);
	
	a = (accept_ack) {2, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered->iid, 1);
	ASSERT_EQ(delivered->ballot, 101);
	free(delivered);
}

TEST_F(LearnerTest, IgnoreOlderBallot) {
	accept_ack a;
	accept_ack* delivered;

	a =	(accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a = (accept_ack) {1, 1, 201, 201, 0, 0};
	learner_receive_accept(l, &a);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered , (accept_ack*)NULL);
	
	a = (accept_ack) {2, 1, 201, 201, 0, 0};
	learner_receive_accept(l, &a);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered->iid, 1);
	ASSERT_EQ(delivered->ballot, 201);
	free(delivered);
}

TEST_F(LearnerTest, NoHoles) {
	accept_ack a;
	accept_ack* delivered;
	iid_t from, to;
	
	a =	(accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {2, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	delivered = learner_deliver_next(l);
	free(delivered);
	
	ASSERT_EQ(learner_has_holes(l, &from, &to), 0);
}

TEST_F(LearnerTest, OneHole) {
	accept_ack a;
	accept_ack* delivered;
	iid_t from, to;
	
	a =	(accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {2, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	delivered = learner_deliver_next(l);
	free(delivered);
	
	a =	(accept_ack) {1, 3, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {2, 3, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	learner_deliver_next(l);
	
	ASSERT_EQ(1, learner_has_holes(l, &from, &to));
	ASSERT_EQ(2, from);
	ASSERT_EQ(3, to);
}

TEST_F(LearnerTest, ManyHoles) {
	accept_ack a;
	accept_ack* delivered;
	iid_t from, to;
	
	a =	(accept_ack) {1, 2, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {2, 2, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	learner_deliver_next(l);
	
	a =	(accept_ack) {1, 100, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {2, 100, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	learner_deliver_next(l);
	
	ASSERT_EQ(1, learner_has_holes(l, &from, &to));
	ASSERT_EQ(1, from);
	ASSERT_EQ(100, to);
}

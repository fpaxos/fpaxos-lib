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

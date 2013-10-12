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
	ASSERT_EQ(NULL, delivered->value.paxos_value_val);
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

#include "gtest.h"
#include "learner.h"

class LearnerTest : public testing::Test {
protected:

	struct learner* l;
	        
	virtual void SetUp() {
		int instances = 100;
		int recover = 1;
		l = learner_new(instances, recover);
	}
	
	virtual void TearDown() { }
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
	
	delivered = learner_deliver_next(l);
	ASSERT_NE(delivered, (accept_ack*)NULL);
	ASSERT_EQ(delivered->iid, 2);
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
}

TEST_F(LearnerTest, LearnMajority) {
	accept_ack a;
	accept_ack* delivered;

	a =	(accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a = (accept_ack) {2, 1, 100, 100, 0, 0};
	learner_receive_accept(l, &a);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered , (accept_ack*)NULL);
	
	a = (accept_ack) {3, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	
	delivered = learner_deliver_next(l);
	ASSERT_EQ(delivered->iid, 1);
	ASSERT_EQ(delivered->ballot, 101);
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
}

TEST_F(LearnerTest, NoHoles) {
	accept_ack a;
	accept_ack* delivered;
	iid_t from, to;
	
	a =	(accept_ack) {1, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	a =	(accept_ack) {2, 1, 101, 101, 0, 0};
	learner_receive_accept(l, &a);
	learner_deliver_next(l);
	
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
	learner_deliver_next(l);
	
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

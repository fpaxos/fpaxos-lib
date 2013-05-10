#include "gtest.h"
#include "learner.h"

class LearnerTest : public testing::Test {
protected:

	int instances;
	struct learner* l;
	        
	virtual void SetUp() {
		instances = 100;
		l = learner_new(instances);
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

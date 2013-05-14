#include "gtest.h"
#include "proposer.h"

class ProposerTest : public testing::Test {
protected:

	int id;
	int instances;
	struct proposer* p;

	virtual void SetUp() {
		id = 2;
		instances = 100;
		p = proposer_new(id, instances);
	}
	
	virtual void TearDown() { }
};

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
	prepare_req pr;
	prepare_ack pa;
	accept_req* ar;
	accept_ack aa;
	
	pr = proposer_prepare(p);

	// aid, iid, bal, val_bal, val_size
	pa = (prepare_ack) {1, pr.iid, pr.ballot, 0, 0};	
	proposer_receive_prepare_ack(p, &pa);
	pa = (prepare_ack) {2, pr.iid, pr.ballot, 0, 0};
	proposer_receive_prepare_ack(p, &pa);
	
	// we have no value to propose!
	ASSERT_EQ(proposer_accept(p), (void*)NULL);
	
	proposer_propose(p, (char*)"value", 6);
	ar = proposer_accept(p);
	ASSERT_NE(ar, (accept_req*)NULL);
	ASSERT_EQ(ar->iid, pr.iid);
	ASSERT_EQ(ar->ballot, pr.ballot);
	ASSERT_EQ(ar->value_size, 6);
	ASSERT_STREQ(ar->value, "value");
	
	// aid, iid, bal, val_bal, final, size
	prepare_req* req;
	aa = (accept_ack) {0, ar->iid, ar->ballot, ar->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);
	aa = (accept_ack) {1, ar->iid, ar->ballot, ar->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);
}

TEST_F(ProposerTest, PreparePreempted) {
	prepare_req pr;
	prepare_ack pa;
	prepare_req* pr_preempt;
	accept_req* ar;
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	pr = proposer_prepare(p);
	proposer_propose(p, value, value_size);
	
	// preempt! proposer receives a different ballot...
	pa = (prepare_ack) {1, pr.iid, pr.ballot+1, 0, 0};
	pr_preempt = proposer_receive_prepare_ack(p, &pa);
	ASSERT_NE((void*)NULL, pr_preempt);
	ASSERT_EQ(pr_preempt->iid, pr.iid);
	ASSERT_GT(pr_preempt->ballot, pr.ballot);
	
	pa = (prepare_ack) {0, pr_preempt->iid, pr_preempt->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	pa = (prepare_ack) {1, pr_preempt->iid, pr_preempt->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	
	ar = proposer_accept(p);
	ASSERT_NE(ar, (void*)NULL);
	ASSERT_EQ(ar->iid, pr_preempt->iid);
	ASSERT_EQ(ar->ballot, pr_preempt->ballot);
	ASSERT_EQ(ar->value_size, value_size);
	ASSERT_STREQ(ar->value, value);
	
	free(ar);
	free(pr_preempt);
}

TEST_F(ProposerTest, AcceptPreempted) {
	prepare_req pr;
	prepare_ack pa;
	prepare_req* pr_preempt;
	accept_req* ar;
	accept_ack aa;
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	pr = proposer_prepare(p);
	proposer_propose(p, value, value_size);
	
	pa = (prepare_ack) {0, pr.iid, pr.ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	pa = (prepare_ack) {1, pr.iid, pr.ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	
	ar = proposer_accept(p);
	ASSERT_NE(ar, (void*)NULL);
	
	// preempt! proposer receives accept nack
	aa = (accept_ack) {0, ar->iid, ar->ballot+1, 0, 0, 0};
	pr_preempt = proposer_receive_accept_ack(p, &aa);	
	ASSERT_NE((void*)NULL, pr_preempt);
	ASSERT_EQ(pr_preempt->iid, pr.iid);
	ASSERT_GT(pr_preempt->ballot, ar->ballot);
	free(ar);
	
	// finally acquire the instance
	pa = (prepare_ack) {0, pr_preempt->iid, pr_preempt->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	pa = (prepare_ack) {1, pr_preempt->iid, pr_preempt->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	
	// accept again
	ar = proposer_accept(p);
	ASSERT_NE(ar, (void*)NULL);
	ASSERT_EQ(pr_preempt->iid, ar->iid);
	ASSERT_EQ(pr_preempt->ballot, ar->ballot);
	ASSERT_EQ(value_size, ar->value_size);
	ASSERT_STREQ(value, ar->value);
	
	aa = (accept_ack) {0, ar->iid, ar->ballot, ar->ballot, 0, value_size};
	ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);
	aa = (accept_ack) {1, ar->iid, ar->ballot, ar->ballot, 0, value_size};
	ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);	
	
	free(ar);
	free(pr_preempt);
}

TEST_F(ProposerTest, ProposedCount) {
	int count = 10;
	prepare_req pr;
	prepare_ack pa;
	accept_req* ar;
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
		pa = (prepare_ack) {0, i+1, pr.ballot, 0, 0};
		proposer_receive_prepare_ack(p, &pa);
		pa = (prepare_ack) {1, i+1, pr.ballot, 0, 0};
		proposer_receive_prepare_ack(p, &pa);
		ar = proposer_accept(p);
		free(ar);
		ASSERT_EQ(count-(i+1), proposer_prepared_count(p));
	}
}

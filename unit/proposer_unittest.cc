#include "gtest.h"
#include "proposer.h"

#define CHECK_ACCEPT_REQ(r, i, b, v, s) { \
	ASSERT_NE(r, (void*)NULL);            \
	ASSERT_EQ(r->iid, i);                 \
	ASSERT_EQ(r->ballot, b);              \
	ASSERT_EQ(r->value_size, s);          \
	ASSERT_STREQ(r->value, v);            \
}

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

prepare_ack* prepare_ack_with_value(prepare_ack pa, char* value, size_t size) {
	prepare_ack* ack = (prepare_ack*)malloc(sizeof(prepare_ack) + size);
	*ack = pa;
	ack->value_size = size;
	memcpy(ack->value, value, size);
	return ack;
}

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
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, "value", 6);
	
	// aid, iid, bal, val_bal, final, size
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
	CHECK_ACCEPT_REQ(ar, pr_preempt->iid, pr_preempt->ballot, value, value_size);
	free(ar);
	free(pr_preempt);
}

TEST_F(ProposerTest, PrepareAlreadyClosed) {
	prepare_req pr;
	prepare_ack* pa;
	prepare_req* pr_preempt;
	accept_req* ar;
	char value[] = "some value";
	int value_size = strlen(value) + 1;
	
	pr = proposer_prepare(p);
	proposer_propose(p, value, value_size);

	// preempt! proposer receives a different ballot...
	pa = prepare_ack_with_value((prepare_ack) {1, pr.iid, pr.ballot+1, 0, 0},
		(char*)"foo bar baz", 12);
	pr_preempt = proposer_receive_prepare_ack(p, pa);
	ASSERT_NE((void*)NULL, pr_preempt);
	ASSERT_EQ(pr_preempt->iid, pr.iid);
	ASSERT_GT(pr_preempt->ballot, pr.ballot);
	free(pa);

	// acquire the instance
	pa = prepare_ack_with_value(
		(prepare_ack){0, pr_preempt->iid, pr_preempt->ballot, 0, 0},
		(char*)"foo bar baz", 12);
	ASSERT_EQ(proposer_receive_prepare_ack(p, pa), (void*)NULL);
	pa->acceptor_id = 1;
	ASSERT_EQ(proposer_receive_prepare_ack(p, pa), (void*)NULL);
	free(pa);

	// proposer has majority with the same value, 
	// we expect the instance to be closed
	ar = proposer_accept(p);
	ASSERT_EQ(ar, (void*)NULL);
	free(pr_preempt);
	
	// try to accept the first value on instance 2
	pr = proposer_prepare(p);
	for (int i = 0; i < QUORUM; ++i) {
		prepare_ack ack = (prepare_ack) {i, pr.iid, pr.ballot, 0, 0};
		ASSERT_EQ(proposer_receive_prepare_ack(p, &ack), (void*)NULL);
	}
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr.iid, pr.ballot, value, value_size)
	free(ar);
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
	
	// check that proposer pushed the instance back 
	// to the prepare phase
	ASSERT_EQ(proposer_prepared_count(p), 1);
	
	// finally acquire the instance
	pa = (prepare_ack) {0, pr_preempt->iid, pr_preempt->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	pa = (prepare_ack) {1, pr_preempt->iid, pr_preempt->ballot, 0, 0};
	ASSERT_EQ(proposer_receive_prepare_ack(p, &pa), (void*)NULL);
	
	// accept again
	ar = proposer_accept(p);
	CHECK_ACCEPT_REQ(ar, pr_preempt->iid, pr_preempt->ballot, value, value_size);
	
	aa = (accept_ack) {0, ar->iid, ar->ballot, ar->ballot, 0, value_size};
	ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);
	aa = (accept_ack) {1, ar->iid, ar->ballot, ar->ballot, 0, value_size};
	ASSERT_EQ(proposer_receive_accept_ack(p, &aa), (void*)NULL);
	
	free(ar);
	free(pr_preempt);
}

TEST_F(ProposerTest, PreparedCount) {
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

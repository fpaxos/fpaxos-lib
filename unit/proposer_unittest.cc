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
	prepare_req pr;
	for (int i = 0; i < 10; ++i) {
		pr = proposer_prepare(p);
		ASSERT_EQ(pr.iid, i+1);
		ASSERT_EQ(pr.ballot, id + MAX_N_OF_PROPOSERS);
	}
}

TEST_F(ProposerTest, ReceivePrepare) {
	iid_t iid;
	ballot_t ballot;
	prepare_req pr;
	prepare_ack pa;
	paxos_msg* value;
	
	pr = proposer_prepare(p);

	// aid, iid, bal, val_bal, val_size
	pa = (prepare_ack) {1, pr.iid, pr.ballot, 0, 0};	
	proposer_receive_prepare(p, &pa);
	pa = (prepare_ack) {2, pr.iid, pr.ballot, 0, 0};
	proposer_receive_prepare(p, &pa);
	
	// we have no value to propose!
	ASSERT_EQ(proposer_accept(p, &iid, &ballot, &value), 0);
	
	proposer_propose(p, (char*)"value", 6);
	ASSERT_NE(proposer_accept(p, &iid, &ballot, &value), 0);
	ASSERT_EQ(value->data_size, 6);
}

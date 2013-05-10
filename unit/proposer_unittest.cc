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
	iid_t iid;
	ballot_t ballot;
	for (int i = 0; i < 10; ++i) {
		proposer_prepare(p, &iid, &ballot);
		ASSERT_EQ(iid, i+1);
		ASSERT_EQ(ballot, id + MAX_N_OF_PROPOSERS);
	}
}

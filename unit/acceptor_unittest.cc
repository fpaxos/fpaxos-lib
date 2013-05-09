#include "gtest.h"
#include "acceptor.h"

class AcceptorTest : public testing::Test {
protected:

	int id;
	struct acceptor* a;
        
	virtual void SetUp() {
		id = 2;
		a = acceptor_new(id);
	}
        
	virtual void TearDown() {
		acceptor_delete(a);
	}
};

TEST_F(AcceptorTest, Prepare) {
	prepare_req p = {1, 0};
	acceptor_record* rec = acceptor_receive_prepare(a, &p);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->acceptor_id, id);
	ASSERT_EQ(rec->iid, 1);
	ASSERT_EQ(rec->ballot, 0);
	ASSERT_EQ(rec->is_final, 0);
	ASSERT_EQ(rec->value_ballot, 0);
	ASSERT_EQ(rec->value_size, 0);
}

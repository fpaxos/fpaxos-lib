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
	prepare_req p = {1, 101};
	acceptor_record* rec = acceptor_receive_prepare(a, &p);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->acceptor_id, id);
	ASSERT_EQ(rec->iid, 1);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->is_final, 0);
	ASSERT_EQ(rec->value_ballot, 0);
	ASSERT_EQ(rec->value_size, 0);
}

TEST_F(AcceptorTest, PrepareDuplicate) {
	prepare_req p = {1, 101};
	acceptor_receive_prepare(a, &p);
	acceptor_record* rec = acceptor_receive_prepare(a, &p);
	ASSERT_EQ(rec , (acceptor_record*)NULL);
}

TEST_F(AcceptorTest, PrepareSmallerBallot) {
	acceptor_record* rec;
	prepare_req p1 = {1, 11};
	prepare_req p2 = {1, 10};
	prepare_req p3 = {1, 0};
	
	rec = acceptor_receive_prepare(a, &p1);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 11);
		
	rec = acceptor_receive_prepare(a, &p2);
	ASSERT_EQ(rec , (acceptor_record*)NULL);
	
	rec = acceptor_receive_prepare(a, &p3);
	ASSERT_EQ(rec , (acceptor_record*)NULL);
}

TEST_F(AcceptorTest, PrepareHigherBallot) {
	acceptor_record* rec;
	prepare_req p1 = {1, 0};
	prepare_req p2 = {1, 10};
	prepare_req p3 = {1, 11};
	
	acceptor_receive_prepare(a, &p1);
	
	rec = acceptor_receive_prepare(a, &p2);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 10);
	
	rec = acceptor_receive_prepare(a, &p3);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 11);
}

TEST_F(AcceptorTest, Accept) {
	acceptor_record* rec;
	accept_req ar = {1, 101, 0};	// no value
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->acceptor_id, id);
	ASSERT_EQ(rec->iid, 1);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->is_final, 0);
	ASSERT_EQ(rec->value_ballot, 101);
	ASSERT_EQ(rec->value_size, 0);
}

TEST_F(AcceptorTest, AcceptPrepared) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 101, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 0);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 101);
}

TEST_F(AcceptorTest, AcceptHigherBallot) {
	acceptor_record* rec;
	prepare_req pr = {1, 101};
	accept_req ar = {1, 201, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 101);
	ASSERT_EQ(rec->value_ballot, 0);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 201);
}

TEST_F(AcceptorTest, AcceptSmallerBallot) {
	acceptor_record* rec;
	prepare_req pr = {1, 201};
	accept_req ar = {1, 101, 0};
	
	rec = acceptor_receive_prepare(a, &pr);
	ASSERT_NE(rec, (acceptor_record*)NULL);
	ASSERT_EQ(rec->ballot, 201);
	ASSERT_EQ(rec->value_ballot, 0);
	
	rec = acceptor_receive_accept(a, &ar);
	ASSERT_EQ(rec, (acceptor_record*)NULL);
}

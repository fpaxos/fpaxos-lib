#include "proposer.h"
#include "carray.h"
#include "quorum.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

struct instance
{
	iid_t iid;
	ballot_t ballot;
	ballot_t value_ballot;
	paxos_msg* value;
	int closed;
	struct quorum prepare_quorum;
	struct quorum accept_quorum;
};

struct proposer 
{
	int id;
	struct carray* values;
	// instances waiting for prepare acks
	iid_t next_prepare_iid;
	struct carray* prepare_instances;
	// instances waiting for accept acks
	// TODO accept_instances should be a hash table
	struct carray* accept_instances;
};


static struct instance* instance_new(iid_t iid, ballot_t ballot);
static void instance_free(struct instance* inst);
static struct instance* instance_find(struct carray* c, iid_t iid);
static int instance_match(void* arg, void* item);
static struct carray* instance_remove(struct carray* c, struct instance* inst);
static paxos_msg* wrap_value(char* value, size_t size);
static prepare_req* prepare_preempt(struct proposer *p, struct instance* inst);
static ballot_t proposer_next_ballot(struct proposer* s, ballot_t b);


struct proposer*
proposer_new(int id, int instances)
{
	struct proposer *p;
	p = malloc(sizeof(struct proposer));
	p->id = id;
	p->values = carray_new(instances);
	p->next_prepare_iid = 0;
	p->prepare_instances = carray_new(instances);
	p->accept_instances = carray_new(instances);
	return p;
}

void
proposer_propose(struct proposer* p, char* value, size_t size)
{
	paxos_msg* msg;
	msg = wrap_value(value, size);
	carray_push_back(p->values, msg);
}

prepare_req
proposer_prepare(struct proposer* p)
{
	struct instance* inst;
	iid_t iid = ++(p->next_prepare_iid);
	inst = instance_new(iid, proposer_next_ballot(p, 0));
	carray_push_back(p->prepare_instances, inst);
	return (prepare_req) {inst->iid, inst->ballot};
}

prepare_req*
proposer_receive_prepare_ack(struct proposer* p, prepare_ack* ack)
{
	struct instance* inst;
	
	inst = instance_find(p->prepare_instances, ack->iid);
	
	if (inst == NULL) {
		LOG(DBG, ("Promise dropped, iid:%u not pending\n", ack->iid));
		return NULL;
	}
	
	if (inst->ballot == ack->ballot) {	// preempted?
		
		if (!quorum_add(&inst->prepare_quorum, ack->acceptor_id)) {
			LOG(DBG, ("Dropping duplicate promise from: %d, iid: %u\n", 
				ack->acceptor_id, inst->iid));
			return NULL;
		}
		
		LOG(DBG, ("Received valid promise from: %d, iid: %u\n",
			ack->acceptor_id, inst->iid));	
		
		if (ack->value_size > 0) {
			LOG(DBG, ("Promise has value\n"));
			if (ack->value_ballot > inst->value_ballot) {
				if (inst->value != NULL)
					free(inst->value);
				inst->ballot = inst->value_ballot;
				inst->value = wrap_value(ack->value, ack->value_size);
				inst->value_ballot = ack->value_ballot;
				LOG(DBG, ("Value in promise saved\n"));
			} else if (ack->value_ballot == inst->value_ballot) {
				// TODO this assumes that the QUORUM is 2!
				LOG(DBG, ("Instance is already closed\n"));
				inst->closed = 1;	
			} else {
				LOG(DBG, ("Value in promise ignored\n"));
			}
		}
		
		return NULL;
		
	} else {
		LOG(DBG, ("Instance %u preempted: ballot %d ack ballot %d\n",
			inst->iid, inst->ballot, ack->ballot));
		return prepare_preempt(p, inst);
	}
}

accept_req* 
proposer_accept(struct proposer* p)
{
	struct instance* inst;

	// is there a prepared instance?
	inst = carray_front(p->prepare_instances);
	
	if (inst == NULL || !quorum_reached(&inst->prepare_quorum)) {
		LOG(DBG, ("No instance prepared\n"));
		return NULL;
	}
	
	LOG(DBG, ("Trying to accept iid %u\n", inst->iid));
	
	// is there a value?
	if (inst->value == NULL) {
		inst->value = carray_pop_front(p->values);
		if (inst->value == NULL) {
			LOG(DBG, ("No value to accept\n"));
			return NULL;	
		}
		LOG(DBG,("Popped next value\n"));
	} else {
		LOG(DBG, ("Instance has value\n"));
	}
	
	if (inst->closed) {
		LOG(DBG, ("Instance already closed\n"));
		carray_pop_front(p->prepare_instances);
		return NULL;
	}
	
	// we have both a prepared instance and a value
	inst = carray_pop_front(p->prepare_instances);
	carray_push_back(p->accept_instances, inst);
	
	accept_req* req = malloc(sizeof(accept_req) + inst->value->data_size);
	req->iid = inst->iid;
	req->ballot = inst->ballot;
	req->value_size = inst->value->data_size;
	memcpy(req->value, inst->value->data, req->value_size);

	return req;
}

prepare_req*
proposer_receive_accept_ack(struct proposer* p, accept_ack* ack)
{
	struct instance* inst;
	
	inst = instance_find(p->accept_instances, ack->iid);
	
	if (inst == NULL) {
		LOG(DBG, ("Accept ack dropped, iid:%u not pending\n", ack->iid));
		return NULL;
	}
	
	if (ack->ballot == inst->ballot) {
		assert(ack->value_ballot == inst->ballot);
		if (!quorum_add(&inst->accept_quorum, ack->acceptor_id)) {
			LOG(DBG, ("Dropping duplicate accept from: %d, iid: %u\n", 
				ack->acceptor_id, inst->iid));
			return NULL;
		}
		
		if (quorum_reached(&inst->accept_quorum)) {
			LOG(DBG, ("Quorum reached for instance %u\n", inst->iid));
			p->accept_instances = instance_remove(p->accept_instances, inst);
			instance_free(inst);
		}
		
		return NULL;
		
	} else {
		LOG(DBG, ("Instance %u preempted: ballot %d ack ballot %d\n",
			inst->iid, inst->ballot, ack->ballot));
		
		p->accept_instances = instance_remove(p->accept_instances, inst);
		carray_push_front(p->prepare_instances, inst);
		
		return prepare_preempt(p, inst);
	}
}

static struct instance*
instance_new(iid_t iid, ballot_t ballot)
{
	struct instance* inst;
	inst = malloc(sizeof(struct instance));
	inst->iid = iid;
	inst->ballot = ballot;
	inst->value_ballot = 0;
	inst->value = NULL;
	inst->closed = 0;
	quorum_init(&inst->prepare_quorum, QUORUM);
	quorum_init(&inst->accept_quorum, QUORUM);
	return inst;
}

static void
instance_free(struct instance* inst)
{
	if (inst->value != NULL)
		free(inst->value);
	free(inst);
}

static struct instance*
instance_find(struct carray* c, iid_t iid) 
{
	int i;
	for (i = 0; i < carray_count(c); ++i) {
		struct instance* inst = carray_at(c, i);
		if (inst->iid == iid)
			return carray_at(c, i);
	}
	return NULL;
}

static int
instance_match(void* arg, void* item)
{
	struct instance* a = arg;
	struct instance* b = item;
    return a->iid == b->iid;
}

static struct carray*
instance_remove(struct carray* c, struct instance* inst)
{
	struct carray* tmp;
	tmp = carray_reject(c, instance_match, inst);
	carray_free(c);
	return tmp;
}

static paxos_msg*
wrap_value(char* value, size_t size)
{
	paxos_msg* msg = malloc(size + sizeof(paxos_msg));
	msg->data_size = size;
	msg->type = submit;
	memcpy(msg->data, value, size);
	return msg;
}

static prepare_req*
prepare_preempt(struct proposer* p, struct instance* inst)
{
	prepare_req* req;
	inst->ballot = proposer_next_ballot(p, inst->ballot);
	quorum_init(&inst->prepare_quorum, QUORUM);
	quorum_init(&inst->accept_quorum, QUORUM);
	req = malloc(sizeof(prepare_req));
	*req = (prepare_req) {inst->iid, inst->ballot};
	return req;
}

static ballot_t 
proposer_next_ballot(struct proposer* s, ballot_t b)
{
	if (b > 0)
		return MAX_N_OF_PROPOSERS + b;
	else
		return MAX_N_OF_PROPOSERS + s->id;
}

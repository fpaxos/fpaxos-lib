//
// Created by Michael Davis on 07/02/2020.
//


#include <paxos_types.h>
#include <paxos.h>
#include <instance.h>
#include <paxos_util.h>
#include <proposer_common.h>

#include <khash.h>
#include <carray.h>

KHASH_MAP_INIT_INT(instance_info, struct epoch_proposer_instance_info*)


struct epoch_proposer {
    int id;
    int acceptors;
    int q1;
    int q2;
    struct carray* values;
    iid_t max_trim_iid;
    iid_t next_prepare_iid;
    khash_t(instance_info)* prepare_proposer_instance_infos; /* Waiting for prepare acks */
    khash_t(instance_info)* accept_proposer_instance_infos;  /* Waiting for accept acks */
};


struct timeout_iterator{
    khiter_t prepare_instance, accept_instance;
    struct timeval timeout;
    struct epoch_proposer* proposer;
};

struct epoch_proposer* epoch_proposer_new(int id, int acceptors, int q1, int q2){
    struct epoch_proposer* proposer = calloc(1, sizeof(struct epoch_proposer));
    proposer->id = id;
    proposer->acceptors = acceptors;
    proposer->q1 = q1;
    proposer->q2 = q2;

    proposer->prepare_proposer_instance_infos = kh_init(instance_info);
    proposer->accept_proposer_instance_infos = kh_init(instance_info);

    proposer->max_trim_iid = 0;
    proposer->next_prepare_iid = 0;
    proposer->values = carray_new(128); // why?
    return proposer;
}

static void epoch_proposer_instance_info_free(struct epoch_proposer_instance_info* inst) {
    proposer_common_instance_info_free(&inst->common_info);
    epoch_quorum_destroy(&inst->epoch_quorum);
}

void epoch_proposer_free(struct epoch_proposer* p){
    struct epoch_proposer_instance_info* inst;
    kh_foreach_value(p->prepare_proposer_instance_infos, inst, epoch_proposer_instance_info_free(inst));
    kh_foreach_value(p->accept_proposer_instance_infos, inst, epoch_proposer_instance_info_free(inst));
    kh_destroy(instance_info, p->prepare_proposer_instance_infos);
    kh_destroy(instance_info, p->accept_proposer_instance_infos);
    carray_foreach(p->values, carray_paxos_value_free);
    carray_free(p->values);
    free(p);
}


void epoch_proposer_propose(struct epoch_proposer* p, const char* value, size_t size);
int epoch_proposer_prepared_count(struct epoch_proposer* p);
void epoch_proposer_set_instance_id(struct epoch_proposer* p, iid_t iid);

// phase 1
void epoch_proposer_prepare(struct epoch_proposer* p, paxos_prepare* out);
void epoch_proposer_epoch_ballot_prepare(struct epoch_proposer* p, struct epoch_ballot_prepare* out);
int epoch_proposer_receive_epoch_ballot_promise(struct epoch_proposer* p, paxos_promise* ack,
                                                paxos_prepare* out); // add to epoch_ballot_quorum

// phase 2
int epoch_proposer_epoch_ballot_accept(struct epoch_proposer* p, paxos_accept* out); //
int epoch_proposer_receive_epoch_ballot_accepted(struct epoch_proposer* p, paxos_accepted* ack); // add to quorum
void epoch_proposer_instance_chosen(struct epoch_proposer* p, struct instance_chosen_at_epoch_ballot* chosen); // increase instance and send to all acceptors and learners



// Out of dateness messages
int epoch_proposer_receive_preempted(struct epoch_proposer* p, paxos_preempted* ack,
                                     paxos_prepare* out); // timeout ++ and ballot increase + acceptor current or epoch
int epoch_proposer_receive_instance_chosen(struct epoch_proposer* p, struct instance_chosen_at_epoch_ballot* chosen); // set current instance ++



// periodic acceptor state
void epoch_proposer_receive_acceptor_state(struct epoch_proposer* p,
                                           paxos_standard_acceptor_state* state);

// timeouts
struct timeout_iterator* epoch_proposer_timeout_iterator(struct epoch_proposer* p);
int timeout_iterator_prepare(struct timeout_iterator* iter, paxos_prepare* out);
int timeout_iterator_accept(struct timeout_iterator* iter, paxos_accept* out);
void timeout_iterator_free(struct timeout_iterator* iter);

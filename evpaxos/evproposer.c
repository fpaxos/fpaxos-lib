/*
 * Copyright (c) 2013-2014, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include "evpaxos.h"
#include "peers.h"
#include "message.h"
#include "proposer.h"
#include <string.h>
#include "khash.h"
#include <stdlib.h>
#include <event2/event.h>
#include "paxos_message_conversion.h"
#include <paxos_types.h>
#include <assert.h>

#define INITIAL_BACKOFF_TIME 5000
#define MAX_BACKOFF_TIME 1000000



KHASH_MAP_INIT_INT(backoffs, struct timeval*)
KHASH_MAP_INIT_INT(retries, iid_t*)


struct evproposer {
	uint32_t id;
	int preexec_window;
	struct proposer* state;
	struct peers* peers;
	struct timeval tv;
	struct event* timeout_ev;

	khash_t(backoffs)* current_backoffs;
	khash_t(retries)*  awaiting_reties;
};

static void
peer_send_chosen(struct peer* p, void* arg){
    send_paxos_chosen(peer_get_buffer(p), arg);
}

static void
peer_send_prepare(struct peer* p, void* arg)
{
	send_paxos_prepare(peer_get_buffer(p), arg);
}

static void
peer_send_accept(struct peer* p, void* arg)
{
	send_paxos_accept(peer_get_buffer(p), arg);
}


// Begins one or more instances, defined by the preexec_window
static void
proposer_preexecute(struct evproposer* p)
{
	int i;
	struct paxos_prepare pr;
	int count = p->preexec_window - proposer_prepared_count(p->state);
   // iid_t next_instance = proposer_get_min_unchosen_instance(p->state);

    if (count <= 0) return;
	for (i = 0; i < count; i++) {
	    iid_t current_instance = proposer_get_next_instance_to_prepare(p->state);//proposer_get_next_instance_to_prepare(p->state);
	    proposer_set_current_instance(p->state, current_instance);
        paxos_log_debug("current proposing instance: %u", current_instance);
		proposer_prepare(p->state, proposer_get_current_instance(p->state), &pr);
		peers_for_n_acceptor(p->peers, peer_send_prepare, &pr, paxos_config.group_1);
	}
	paxos_log_debug("Opened %d new instances", count);
}


static void
try_accept(struct evproposer* p)
{
	paxos_accept accept;
	while (proposer_accept(p->state, &accept)) {
        assert(&accept.value != NULL);
        assert(accept.value.paxos_value_val != NULL);
        assert(accept.value.paxos_value_len >0);
        peers_for_n_acceptor(p->peers, peer_send_accept, &accept, paxos_config.group_2);
    }
	proposer_preexecute(p);
}

static void
evproposer_handle_promise(struct peer* p, standard_paxos_message* msg, void* arg)
{
	struct evproposer* proposer = arg;
	paxos_prepare prepare;
	paxos_promise* promise = &msg->u.promise;

	if (promise->ballot.proposer_id != proposer->id)
	    return;

	int quorum_reached = proposer_receive_promise(proposer->state, promise, &prepare);
	if (quorum_reached)
	    try_accept(proposer);
}

static void
evproposer_handle_accepted(struct peer* p, standard_paxos_message* msg, void* arg)
{
	struct evproposer* proposer = arg;
	paxos_accepted* acc = &msg->u.accepted;

	if (acc->promise_ballot.proposer_id != proposer->id) return; // todo fix instance info so that proposers can handle other proposals instead of just their last one

    struct paxos_chosen chosen_msg;
    memset(&chosen_msg, 0, sizeof(struct paxos_chosen));

    if (proposer_receive_accepted(proposer->state, acc, &chosen_msg)){
        assert(chosen_msg.iid == acc->iid);
        assert(ballot_equal(&chosen_msg.ballot, acc->promise_ballot));
        assert(ballot_equal(&chosen_msg.ballot, acc->value_ballot));
        assert(is_values_equal(chosen_msg.value, acc->value));
   //     peers_foreach_acceptor(proposer->peers, peer_send_chosen, &chosen_msg);
        peers_foreach_client(proposer->peers, peer_send_chosen, &chosen_msg);
	//	proposer_preexecute(proposer);
	try_accept(proposer);
    //    proposer_preexecute(proposer);
    }
}

static void
evproposer_handle_chosen(struct peer* p, struct standard_paxos_message* msg, void* arg) {
    struct evproposer* proposer = arg;
    struct paxos_chosen* chosen_msg = &msg->u.chosen;
    proposer_receive_chosen(proposer->state, chosen_msg);

    khiter_t key = kh_get_backoffs(proposer->current_backoffs, chosen_msg->iid);

    if (key != kh_end(proposer->current_backoffs)) {
        kh_del_backoffs(proposer->current_backoffs, key);
    }
    try_accept(proposer);
  // proposer_preexecute(proposer);
//    proposer_preexecute(proposer);
}

struct retry {
    struct evproposer* proposer;
    struct paxos_prepare* prepare;
    iid_t instance;
};

static void
evproposer_try_higher_ballot(evutil_socket_t fd, short event, void* arg) {
    struct retry* args =  arg;
    struct evproposer* proposer = args->proposer;
    iid_t instance = args->instance;
  //  struct paxos_prepare next_prepare;

    paxos_log_debug("Trying next ballot %u, %u.%u", args->prepare->iid, args->prepare->ballot.number, args->prepare->ballot.proposer_id);
    peers_for_n_acceptor(proposer->peers, peer_send_prepare, args->prepare, paxos_config.group_1);

   khiter_t key = kh_get_retries(proposer->awaiting_reties, instance);

    if (key != kh_end(proposer->awaiting_reties))
        kh_del_retries(proposer->awaiting_reties, key);
   paxos_prepare_free(args->prepare);
   try_accept(proposer);
}


int get_initial_backoff() { return 1 + (rand() % INITIAL_BACKOFF_TIME); }

unsigned int get_next_backoff(const unsigned int old_time) {
    //unsigned  int new_time = (old_time << (unsigned int) 1) % MAX_BACKOFF_TIME;
  //  if (new_time == 0) {
  //      new_time = get_initial_backoff();
   // }
  //  return new_time;
    return old_time;
}


static void
evproposer_handle_preempted(struct peer* p, standard_paxos_message* msg, void* arg)
{
    struct evproposer* proposer = arg;
    struct paxos_preempted preempted_msg = msg->u.preempted;

    if (preempted_msg.ballot.proposer_id != proposer->id) return;


    struct paxos_prepare* next_prepare = calloc(1, sizeof(struct paxos_prepare));

    if (proposer_receive_preempted(proposer->state, &preempted_msg, next_prepare)) {

        paxos_log_debug("Next ballot to try %u, %u.%u", next_prepare->iid, next_prepare->ballot.number, next_prepare->ballot.proposer_id);

        khiter_t retries_key = kh_get_retries(proposer->awaiting_reties, preempted_msg.iid);
        if (retries_key != kh_end(proposer->awaiting_reties)) { // already queued
            paxos_log_debug("Preempted disregarded, it is already backing off");
            return;
        }

        khiter_t backoffs_key = kh_get_backoffs(proposer->current_backoffs, preempted_msg.iid);

        struct timeval* backoff_new;
        // check if backoff is existing
        if (backoffs_key == kh_end(proposer->current_backoffs)) {
            // first preempt
            backoff_new = calloc(1, sizeof(struct timeval));
            backoff_new->tv_sec = 0;
            backoff_new->tv_usec = get_initial_backoff();

            int success;
            backoffs_key = kh_put_backoffs(proposer->current_backoffs, preempted_msg.iid, &success);
            assert(success > 0);
            kh_value(proposer->current_backoffs, backoffs_key) = backoff_new;
        } else {
            // next preempt
            backoff_new = kh_value(proposer->current_backoffs, backoffs_key);
            backoff_new->tv_usec = get_next_backoff(backoff_new->tv_usec);
            paxos_log_debug("Backoff time %u", backoff_new->tv_usec);
            kh_value(proposer->current_backoffs, backoffs_key) = backoff_new;
        }


        // add new event to send after backoff
        struct retry* retry_args = calloc(1, sizeof(struct retry));
        *retry_args = (struct retry) {.proposer = proposer, .prepare = next_prepare, .instance = preempted_msg.iid};

        struct event* ev = evtimer_new(peers_get_event_base(proposer->peers), evproposer_try_higher_ballot, retry_args);
        event_add(ev, backoff_new);
    }
}

static void
evproposer_handle_client_value(struct peer* p, standard_paxos_message* msg, void* arg)
{
	struct evproposer* proposer = arg;
	struct paxos_client_value* v = &msg->u.client_value;
    proposer_add_client_value_to_queue(proposer->state,
                                       v->value.paxos_value_val,
                                       v->value.paxos_value_len);
	try_accept(proposer);
}

static void
evproposer_handle_acceptor_state(struct peer* p, standard_paxos_message* msg, void* arg)
{
	struct evproposer* proposer = arg;
	struct paxos_standard_acceptor_state* acc_state = &msg->u.state;
	proposer_receive_acceptor_state(proposer->state, acc_state);
}

static void
evproposer_check_timeouts(evutil_socket_t fd, short event, void *arg)
{
	struct evproposer* p = arg;
	struct timeout_iterator* iter = proposer_timeout_iterator(p->state);


	paxos_prepare pr;

	while (timeout_iterator_prepare(iter, &pr)) {
		paxos_log_info("Instance %d timed out in phase 1.", pr.iid);
		peers_for_n_acceptor(p->peers, peer_send_prepare, &pr, paxos_config.group_1);
	}

	paxos_accept ar;
	while (timeout_iterator_accept(iter, &ar)) {
		paxos_log_info("Instance %d timed out in phase 2.", ar.iid);
		peers_for_n_acceptor(p->peers, peer_send_accept, &ar, paxos_config.group_2);
	}

	timeout_iterator_free(iter);
	event_add(p->timeout_ev, &p->tv);
}

static void
evproposer_preexec_once(evutil_socket_t fd, short event, void *arg)
{
	struct evproposer* p = arg;
	proposer_preexecute(p);
}

struct evproposer*
evproposer_init_internal(int id, struct evpaxos_config* c, struct peers* peers)
{
	struct evproposer* p;
	int acceptor_count = evpaxos_acceptor_count(c);

	p = malloc(sizeof(struct evproposer));
	p->id = id;
	p->preexec_window = paxos_config.proposer_preexec_window;

	p->current_backoffs = kh_init_backoffs();
	p->awaiting_reties = kh_init_retries();

	peers_subscribe(peers, PAXOS_PROMISE, evproposer_handle_promise, p);
	peers_subscribe(peers, PAXOS_ACCEPTED, evproposer_handle_accepted, p);
	peers_subscribe(peers, PAXOS_PREEMPTED, evproposer_handle_preempted, p);
	peers_subscribe(peers, PAXOS_CLIENT_VALUE, evproposer_handle_client_value, p);
	peers_subscribe(peers, PAXOS_ACCEPTOR_STATE,
                    evproposer_handle_acceptor_state, p);
	peers_subscribe(peers, PAXOS_CHOSEN, evproposer_handle_chosen, p);

	// Setup timeout
	struct event_base* base = peers_get_event_base(peers);
	p->tv.tv_sec = paxos_config.proposer_timeout;
	p->tv.tv_usec = 0;
	p->timeout_ev = evtimer_new(base, evproposer_check_timeouts, p);
	event_add(p->timeout_ev, &p->tv);

	p->state = proposer_new(p->id, acceptor_count,paxos_config.quorum_1,paxos_config.quorum_2);
	p->peers = peers;

    // This initiates the first Paxos Event in the Proposer-Acceptor communication
	event_base_once(base, 0, EV_TIMEOUT, evproposer_preexec_once, p, NULL);

	return p;
}

struct evproposer*
evproposer_init(int id, const char* config_file, struct event_base* base)
{
	struct evpaxos_config* config = evpaxos_config_read(config_file);

	if (config == NULL)
		return NULL;

	// Check id validity of proposer_id
	if (id < 0 || id >= MAX_N_OF_PROPOSERS) {
		paxos_log_error("Invalid proposer id: %d", id);
		return NULL;
	}

	struct peers* peers = peers_new(base, config);
	//struct peers* peers_proposers = peers_new(base, config);

	peers_connect_to_acceptors(peers);
   // peers_connect_to_proposers(peers);

	int port = evpaxos_proposer_listen_port(config, id);
	int rv = peers_listen(peers, port);
	if (rv == 0 ) // failure
		return NULL;

	struct evproposer* p = evproposer_init_internal(id, config, peers);
	evpaxos_config_free(config);
	return p;
}

void
evproposer_free_internal(struct evproposer* p)
{
	event_free(p->timeout_ev);
	proposer_free(p->state);
	free(p);
}

void
evproposer_free(struct evproposer* p)
{
	peers_free(p->peers);
	evproposer_free_internal(p);
}


void
evproposer_set_instance_id(struct evproposer* p, unsigned iid) {
    proposer_set_current_instance(p->state, iid);
}


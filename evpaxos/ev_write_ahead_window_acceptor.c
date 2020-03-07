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
#include "standard_stable_storage.h"
#include "peers.h"
#include "writeahead_window_acceptor.h"
#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <event2/event.h>
#include <evdns.h>
#include <paxos_types.h>
#include "paxos_message_conversion.h"


struct ev_write_ahead_acceptor
{
    struct peers* peers_proposers;
   // struct peers* peers_acceptors;

    struct writeahead_window_acceptor* state;
    struct event* send_state_event;
    struct timeval send_state_timer;

    struct event *instance_window_check_event;
    struct timeval instance_window_check_timer;

    struct event* instance_window_epoch_iteration_event;
    struct timeval instance_window_epoch_iteration_timer;

    struct event* ballot_window_check_event;
    struct timeval ballot_window_check_timer;
    struct event* ballot_window_iteration_event;
    struct timeval ballot_window_iteration_timer;

};

static void
peer_send_paxos_message(struct peer* p, void* arg)
{
    send_paxos_message(peer_get_buffer(p), arg);
}

/*
	Received a prepare request (phase 1a).
*/
static void
ev_write_ahead_acceptor_handle_prepare(struct peer* p, standard_paxos_message* msg, void* arg)
{
    standard_paxos_message out;
    paxos_prepare* prepare = &msg->u.prepare;
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*)arg;
    paxos_log_debug("Handle prepare for iid %d ballot %u.%u",
                    prepare->iid, prepare->ballot.number, prepare->ballot.proposer_id);
    if (write_ahead_window_acceptor_receive_prepare(a->state, prepare, &out) != 0) {

        if (out.type == PAXOS_PROMISE) {
            assert(ballot_equal(&prepare->ballot, out.u.promise.ballot));
            send_paxos_promise(peer_get_buffer(p), &out.u.promise);
        } else if (out.type == PAXOS_CHOSEN) {
            send_paxos_chosen(peer_get_buffer(p), &out.u.chosen);
        } else if (out.type == PAXOS_PREEMPTED){
            send_paxos_preempted(peer_get_buffer(p), &out.u.preempted);
        }

        paxos_message_destroy(&out);
    }

}

/*
	Received a accept request (phase 2a).
*/
static void
ev_write_ahead_acceptor_handle_accept(struct peer* p, standard_paxos_message* msg, void* arg)
{
    standard_paxos_message out;
    paxos_accept* accept = &msg->u.accept;
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*)arg;

    assert(accept->value.paxos_value_len > 0);

    paxos_log_debug("Handle accept for iid %dballot %u.%u",
                    accept->iid, accept->ballot.number, accept->ballot.proposer_id);
    if (write_ahead_window_acceptor_receive_accept(a->state, accept, &out) != 0) {


        if (out.type == PAXOS_ACCEPTED) {
            assert(ballot_equal(&out.u.accepted.promise_ballot, accept->ballot));
            assert(ballot_equal(&out.u.accepted.value_ballot, accept->ballot));
            assert(out.u.accepted.value.paxos_value_len > 0);
            peers_foreach_client(a->peers_proposers,  peer_send_paxos_message, &out);
        //   peers_foreach_proposer(a->peers_proposers, peer_send_paxos_message, &out);
        } else if (out.type == PAXOS_PREEMPTED) {
            send_paxos_preempted(peer_get_buffer(p), &out.u.preempted);
        } else if (out.type == PAXOS_CHOSEN) {
            assert(out.u.chosen.value.paxos_value_len > 0);
            send_paxos_chosen(peer_get_buffer(p), &out.u.chosen);
           //send_paxos_message(peer_get_buffer(p), &out);
        }

        paxos_message_destroy(&out);
    }
}

static void
ev_write_ahead_acceptor_handle_repeat(struct peer* p, standard_paxos_message* msg, void* arg)
{
    iid_t iid;
    struct standard_paxos_message out_msg;
    struct paxos_repeat* repeat = &msg->u.repeat;
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*)arg;
    paxos_log_debug("Handle repeat for iids %d-%d", repeat->from, repeat->to);
    for (iid = repeat->from; iid <= repeat->to; ++iid) {
        if (write_ahead_window_acceptor_receive_repeat(a->state, iid, &out_msg)) {
            if (out_msg.type == PAXOS_ACCEPTED) {
                send_paxos_accepted(peer_get_buffer(p), &out_msg.u.accepted);
                paxos_accepted_destroy(&out_msg.u.accepted);
            } else if (out_msg.type == PAXOS_CHOSEN){
                send_paxos_chosen(peer_get_buffer(p), &out_msg.u.chosen);
                paxos_value_free(&out_msg.u.chosen.value);
            }
        }
    }
}


static void
ev_write_ahead_acceptor_handle_chosen(struct peer* p, struct standard_paxos_message* msg, void* arg){
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*)arg;
    struct paxos_chosen* chosen_msg = &msg->u.chosen;
    paxos_log_debug("Recieved chosen message for instace %u", chosen_msg->iid);
    //acceptor_cho
    write_ahead_ballot_acceptor_receive_chosen(a->state, chosen_msg);
}

static void
ev_write_ahead_acceptor_handle_trim(struct peer* p, standard_paxos_message* msg, void* arg)
{
    paxos_trim* trim = &msg->u.trim;
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*)arg;
    write_ahead_window_acceptor_receive_trim(a->state, trim);
}


static void
send_acceptor_state(int fd, short ev, void* arg)
{
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*)arg;
    standard_paxos_message msg = {.type = PAXOS_ACCEPTOR_STATE};
    write_ahead_window_acceptor_get_current_state(a->state, &msg.u.state);
    peers_foreach_client(a->peers_proposers, peer_send_paxos_message, &msg);
    event_add(a->send_state_event, &a->send_state_timer);
}

static void write_ballot_event(int fd, short ev, void* arg) {
    struct ev_write_ahead_acceptor* a = arg;
    write_ahead_acceptor_write_ballot_window(a->state);
    event_add(a->ballot_window_check_event, &a->ballot_window_check_timer);
}

static void
check_ballot_window_event(int fd, short ev, void* arg) {
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*) arg;

    if (write_ahead_acceptor_check_ballot_window(a->state))
        event_add(a->ballot_window_iteration_event, &a->ballot_window_iteration_timer);
    else
        event_add(a->instance_window_check_event, &a->instance_window_check_timer);
}

static void
write_instance_epoch_event(int fd, short ev, void* arg) {
    struct ev_write_ahead_acceptor* a= (struct ev_write_ahead_acceptor*) arg;
    write_ahead_acceptor_write_iteration_of_instance_epoch(a->state);
    if (write_ahead_acceptor_is_writing_epoch(a->state)) {
        event_add(a->instance_window_epoch_iteration_event, &a->instance_window_epoch_iteration_timer);
    } else {
        event_add(a->instance_window_check_event, &a->instance_window_check_timer);
    }

}

static void
check_instance_epoch_event(int fd, short ev, void* arg) {
    struct ev_write_ahead_acceptor* a = (struct ev_write_ahead_acceptor*) arg;
    if (!write_ahead_acceptor_is_writing_epoch(a->state)) {
        if (write_ahead_acceptor_is_new_instance_epoch_needed(a->state)) {
            write_ahead_acceptor_begin_writing_instance_epoch(a->state); // necessary variable adjustment
            write_ahead_acceptor_write_iteration_of_instance_epoch(a->state); //write the first iteration
            event_add(a->instance_window_epoch_iteration_event, &a->instance_window_epoch_iteration_timer);
        } else {
            event_add(a->instance_window_check_event, &a->instance_window_check_timer);
        }
    }
}

struct ev_write_ahead_acceptor*
ev_write_ahead_acceptor_init_internal(int id, struct evpaxos_config* c, struct peers* peers_proposers)
{
    struct ev_write_ahead_acceptor* acceptor = calloc(1, sizeof(struct ev_write_ahead_acceptor));
    // volatile storage

    // stable storage
    // stable storage duplicate
    // min instance catach up
    // min ballot catachup
    // ballot window
    // instance window


   acceptor->state = write_ahead_window_acceptor_new(id,
            1,
            1,
            500,
            500,
            1);
   // by making instance window less than min instance
   // catchup you can do a sort of write a little bit at once ahead
   // of time rather than a giant bulk-write of all the written ahead instances

    acceptor->peers_proposers = peers_proposers;

    peers_subscribe(peers_proposers, PAXOS_PREPARE, ev_write_ahead_acceptor_handle_prepare, acceptor);
    peers_subscribe(peers_proposers, PAXOS_ACCEPT, ev_write_ahead_acceptor_handle_accept, acceptor);
    peers_subscribe(peers_proposers, PAXOS_REPEAT, ev_write_ahead_acceptor_handle_repeat, acceptor);
    peers_subscribe(peers_proposers, PAXOS_TRIM, ev_write_ahead_acceptor_handle_trim, acceptor);
    peers_subscribe(peers_proposers, PAXOS_CHOSEN, ev_write_ahead_acceptor_handle_chosen, acceptor);


    struct event_base* base = peers_get_event_base(peers_proposers);


 //   acceptor->send_state_event = evtimer_new(base, send_acceptor_state, acceptor);
//    acceptor->send_state_timer = (struct timeval){1, 0};
  //  event_add(acceptor->send_state_event, &acceptor->send_state_timer);

    // New event to check windows async
   // acceptor->instance_window_check_timer = evtimer_new(base, )evti
    acceptor->instance_window_check_event = event_new(base, -1, EV_TIMEOUT, check_instance_epoch_event, acceptor);
    acceptor->instance_window_check_timer = (struct timeval) {.tv_sec = 3 + (rand() % 3), .tv_usec = 0};

    acceptor->instance_window_epoch_iteration_event =  event_new(base, -1, EV_TIMEOUT, write_instance_epoch_event, acceptor);
    acceptor->instance_window_epoch_iteration_timer = (struct timeval) {.tv_sec = 0, .tv_usec = 50000 + (rand() % 1000)}; //0.5 seconds = 500000  us

    event_add(acceptor->instance_window_check_event, &acceptor->instance_window_check_timer);

    acceptor->ballot_window_check_event = evtimer_new(base, check_ballot_window_event, acceptor);
    acceptor->ballot_window_check_timer = (struct timeval) {.tv_sec = 0, .tv_usec = 5000 + (rand() % 1000)};

    acceptor->ballot_window_iteration_event = evtimer_new(base, write_ballot_event, acceptor);
    acceptor->ballot_window_iteration_timer = (struct timeval) {.tv_sec = 0, .tv_usec = 1000};
    // todo add ballot checking and updating event


   // event_set(&acceptor->send_state_event, 0, EV_PERSIST, write_ahead_window_acceptor_check_and_update_write_ahead_windows, acceptor->state);
  //  evtimer_add(&acceptor->send_state_event, &time);

    return acceptor;
}
struct ev_write_ahead_acceptor*
ev_write_ahead_window_acceptor_init(int id, const char* config_file, struct event_base* b)
{
    struct evpaxos_config* config = evpaxos_config_read(config_file);
    if (config  == NULL)
        return NULL;


    int acceptor_count = evpaxos_acceptor_count(config);
    if (id < 0 || id >= acceptor_count) {
        paxos_log_error("Invalid acceptor id: %d.", id);
        paxos_log_error("Should be between 0 and %d", acceptor_count);
        evpaxos_config_free(config);
        return NULL;
    }

    struct peers* peers_proposers = peers_new(b, config);
//    struct peers* peers_acceptors = peers_new(b, config);

    int port = evpaxos_acceptor_listen_port(config, id);

    if (peers_listen(peers_proposers, port) == 0)
        return NULL;


    //todo ask if there are any instances chosen


    // TODO Determine whether or not this should be the place to work out way to handle different acceptors
    struct ev_write_ahead_acceptor* acceptor = ev_write_ahead_acceptor_init_internal(id, config, peers_proposers);
    evpaxos_config_free(config);
    return acceptor;
}

void
ev_write_ahead_acceptor_free_internal(struct ev_write_ahead_acceptor* a)
{
    event_free(a->send_state_event);
    ev_write_ahead_window_acceptor_free(a);
    free(a);
}

void
ev_write_ahead_window_acceptor_free(struct ev_write_ahead_acceptor* a)
{
    peers_free(a->peers_proposers);
    ev_write_ahead_acceptor_free_internal(a);
}

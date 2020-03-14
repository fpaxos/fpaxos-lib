/*
 * Copyright (c) 2013-2015, University of Lugano
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


#include <paxos_types.h>
#include "paxos_types_pack.h"
#include "paxos_value.h"

#define MSGPACK_OBJECT_AT(obj, i) (obj->via.array.ptr[i].via)


static void msgpack_pack_string(msgpack_packer* p, char* buffer, int len)
{
	#if MSGPACK_VERSION_MAJOR > 0
	msgpack_pack_bin(p, len);
	msgpack_pack_bin_body(p, buffer, len);
	#else
	msgpack_pack_raw(p, len);
	msgpack_pack_raw_body(p, buffer, len);
	#endif
}

static void msgpack_unpack_uint32_at(msgpack_object* o, uint32_t* v, int* i)
// NOTE: also increments the value of i as a side effect
{
	*v = (uint32_t)MSGPACK_OBJECT_AT(o,*i).u64;
	(*i)++;
}

static void msgpack_unpack_string_at(msgpack_object* o, char** buffer, int* len, int* i)
// NOTE: also increments the value of i as a side effect
{
	*buffer = NULL;
	#if MSGPACK_VERSION_MAJOR > 0
	*len = MSGPACK_OBJECT_AT(o,*i).bin.size;
	const char* obj = MSGPACK_OBJECT_AT(o,*i++).bin.ptr;
	#else
	*len = MSGPACK_OBJECT_AT(o,*i).raw.size;
	const char* obj = MSGPACK_OBJECT_AT(o,*i).raw.ptr;
	#endif
	if (*len > 0) {
		*buffer = malloc(*len);
		memcpy(*buffer, obj, *len);
	}
	(*i)++;
}



static void msgpack_unpack_ballot(msgpack_object* msg_obejct, struct ballot* ballot, int* i) {
    msgpack_unpack_uint32_at(msg_obejct, &ballot->number, i);
    msgpack_unpack_uint32_at(msg_obejct, &ballot->proposer_id, i);
}

static void msgpack_pack_ballot(msgpack_packer* packer, struct ballot ballot){
    msgpack_pack_uint32(packer, ballot.number);
    msgpack_pack_uint32(packer, ballot.proposer_id);
}


static void msgpack_pack_paxos_value(msgpack_packer* p, struct paxos_value* v)
{
	msgpack_pack_string(p, v->paxos_value_val, (int) v->paxos_value_len);
}

static void msgpack_unpack_paxos_value_at(msgpack_object* o, struct paxos_value* v, int* i)
{
	msgpack_unpack_string_at(o, &v->paxos_value_val, (int*) &v->paxos_value_len, i);
}

void msgpack_pack_paxos_prepare(msgpack_packer* p, struct paxos_prepare* v)
{
	msgpack_pack_array(p, 4);
	msgpack_pack_int32(p, PAXOS_PREPARE);
	msgpack_pack_uint32(p, v->iid);
    msgpack_pack_ballot(p, v->ballot);
}

void msgpack_unpack_paxos_prepare(msgpack_object* o, struct paxos_prepare* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_ballot(o, &v->ballot, &i);
}

void msgpack_pack_paxos_promise(msgpack_packer* p, paxos_promise* v)
{
	msgpack_pack_array(p, 8);
	msgpack_pack_int32(p, PAXOS_PROMISE);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_ballot(p, v->ballot);
	msgpack_pack_ballot(p, v->value_ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_promise(msgpack_object* o, paxos_promise* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_ballot(o, &v->ballot, &i);
	msgpack_unpack_ballot(o, &v->value_ballot, &i);
	msgpack_unpack_paxos_value_at(o, &v->value, &i);
}

void msgpack_pack_paxos_accept(msgpack_packer* p, paxos_accept* v)
{
	msgpack_pack_array(p, 5);
	msgpack_pack_int32(p, PAXOS_ACCEPT);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_ballot(p, v->ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_accept(msgpack_object* o, paxos_accept* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_ballot(o, &v->ballot, &i);
	msgpack_unpack_paxos_value_at(o, &v->value, &i);
}

void msgpack_pack_paxos_accepted(msgpack_packer* p, paxos_accepted* v)
{
	msgpack_pack_array(p, 8);
	msgpack_pack_int32(p, PAXOS_ACCEPTED);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_ballot(p, v->promise_ballot);
	msgpack_pack_ballot(p, v->value_ballot);
	msgpack_pack_paxos_value(p, &v->value);
}

void msgpack_unpack_paxos_accepted(msgpack_object* o, paxos_accepted* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_ballot(o, &v->promise_ballot, &i);
	msgpack_unpack_ballot(o, &v->value_ballot, &i);
	msgpack_unpack_paxos_value_at(o, &v->value, &i);
}

void msgpack_pack_paxos_preempted(msgpack_packer* p, paxos_preempted* v)
{
	msgpack_pack_array(p, 5);
	msgpack_pack_int32(p, PAXOS_PREEMPTED);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->iid);
	msgpack_pack_ballot(p, v->ballot);
}

void msgpack_unpack_paxos_preempted(msgpack_object* o, paxos_preempted* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->iid, &i);
	msgpack_unpack_ballot(o, &v->ballot, &i);
}

void msgpack_pack_paxos_repeat(msgpack_packer* p, paxos_repeat* v)
{
	msgpack_pack_array(p, 3);
	msgpack_pack_int32(p, PAXOS_REPEAT);
	msgpack_pack_uint32(p, v->from);
	msgpack_pack_uint32(p, v->to);
}

void msgpack_unpack_paxos_repeat(msgpack_object* o, paxos_repeat* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->from, &i);
	msgpack_unpack_uint32_at(o, &v->to, &i);
}

void msgpack_pack_paxos_trim(msgpack_packer* p, paxos_trim* v)
{
	msgpack_pack_array(p, 2);
	msgpack_pack_int32(p, PAXOS_TRIM);
	msgpack_pack_uint32(p, v->iid);
}

void msgpack_unpack_paxos_trim(msgpack_object* o, paxos_trim* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->iid, &i);
}

void msgpack_pack_paxos_acceptor_state(msgpack_packer* p, paxos_standard_acceptor_state* v)
{
	msgpack_pack_array(p, 3);
	msgpack_pack_int32(p, PAXOS_ACCEPTOR_STATE);
	msgpack_pack_uint32(p, v->aid);
	msgpack_pack_uint32(p, v->trim_iid);
}

void msgpack_unpack_paxos_acceptor_state(msgpack_object* o, paxos_standard_acceptor_state* v)
{
	int i = 1;
	msgpack_unpack_uint32_at(o, &v->aid, &i);
	msgpack_unpack_uint32_at(o, &v->trim_iid, &i);
}

void msgpack_pack_paxos_client_value(msgpack_packer* p, struct paxos_value* v)
{
	msgpack_pack_array(p, 2);
	msgpack_pack_int32(p, PAXOS_CLIENT_VALUE);
	msgpack_pack_paxos_value(p, v);
}

void msgpack_unpack_paxos_client_value(msgpack_object* o, struct paxos_value* v)
{
	int i = 1;
	msgpack_unpack_paxos_value_at(o, v, &i);
}


void msgpack_unpack_paxos_chosen(msgpack_object* msg_object, struct paxos_chosen* unpacked_chosen_msg){
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &unpacked_chosen_msg->iid, &i);
    msgpack_unpack_ballot(msg_object, &unpacked_chosen_msg->ballot, &i);
    msgpack_unpack_paxos_value_at(msg_object, &unpacked_chosen_msg->value, &i);
}


void msgpack_pack_paxos_chosen(msgpack_packer* packer, struct paxos_chosen* instance_chosen) {
    msgpack_pack_array(packer, 5);
    msgpack_pack_uint32(packer, PAXOS_CHOSEN);
    msgpack_pack_uint32(packer, instance_chosen->iid);
    msgpack_pack_ballot(packer, instance_chosen->ballot);
    msgpack_pack_paxos_value(packer, &instance_chosen->value);
}

void msgpack_pack_paxos_message(msgpack_packer* p, standard_paxos_message* v)
{
	switch (v->type) {
        case PAXOS_PREPARE:
            msgpack_pack_paxos_prepare(p, &v->u.prepare);
            break;
        case PAXOS_PROMISE:
            msgpack_pack_paxos_promise(p, &v->u.promise);
            break;
        case PAXOS_ACCEPT:
            msgpack_pack_paxos_accept(p, &v->u.accept);
            break;
        case PAXOS_ACCEPTED:
            msgpack_pack_paxos_accepted(p, &v->u.accepted);
            break;
        case PAXOS_PREEMPTED:
            msgpack_pack_paxos_preempted(p, &v->u.preempted);
            break;
        case PAXOS_REPEAT:
            msgpack_pack_paxos_repeat(p, &v->u.repeat);
            break;
        case PAXOS_TRIM:
            msgpack_pack_paxos_trim(p, &v->u.trim);
            break;
        case PAXOS_ACCEPTOR_STATE:
            msgpack_pack_paxos_acceptor_state(p, &v->u.state);
            break;
        case PAXOS_CLIENT_VALUE:
            msgpack_pack_paxos_client_value(p, &v->u.client_value);
            break;
        case PAXOS_CHOSEN:
            msgpack_pack_paxos_chosen(p, &v->u.chosen);
            break;
    }
}

void msgpack_unpack_paxos_message(msgpack_object* o, standard_paxos_message* v)
{
	v->type = MSGPACK_OBJECT_AT(o,0).u64;
	switch (v->type) {
	case PAXOS_PREPARE:
		msgpack_unpack_paxos_prepare(o, &v->u.prepare);
		break;
	case PAXOS_PROMISE:
		msgpack_unpack_paxos_promise(o, &v->u.promise);
		break;
	case PAXOS_ACCEPT:
		msgpack_unpack_paxos_accept(o, &v->u.accept);
		break;
	case PAXOS_ACCEPTED:
		msgpack_unpack_paxos_accepted(o, &v->u.accepted);
		break;
	case PAXOS_PREEMPTED:
		msgpack_unpack_paxos_preempted(o, &v->u.preempted);
		break;
	case PAXOS_REPEAT:
		msgpack_unpack_paxos_repeat(o, &v->u.repeat);
		break;
	case PAXOS_TRIM:
		msgpack_unpack_paxos_trim(o, &v->u.trim);
		break;
	case PAXOS_ACCEPTOR_STATE:
		msgpack_unpack_paxos_acceptor_state(o, &v->u.state);
		break;
	case PAXOS_CLIENT_VALUE:
		msgpack_unpack_paxos_client_value(o, &v->u.client_value);
		break;
    case PAXOS_CHOSEN:
        msgpack_unpack_paxos_chosen(o, &v->u.chosen);
        break;
	}
}


// WRITEAHEAD EPOCH PAXOS STUFF
// TODO get rid of magic numers
static void msgpack_pack_epoch_ballot(msgpack_packer* packer, struct epoch_ballot* epoch_bal){
   msgpack_pack_uint32(packer, epoch_bal->epoch);
   msgpack_pack_ballot(packer, epoch_bal->ballot);
 //  msgpack_pack_uint32(packer, epoch_bal->ballot);
}

// For these packing methods you need to ensure that 2 array indexes are given for each epoch ballot

void msgpack_pack_epoch_ballot_prepare(msgpack_packer* packer, struct epoch_ballot_prepare* prepare){
    msgpack_pack_array(packer, 4);
    msgpack_pack_uint32(packer, WRITEAHEAD_EPOCH_BALLOT_PREPARE);
    msgpack_pack_uint32(packer, prepare->instance);
    msgpack_pack_epoch_ballot(packer, &prepare->epoch_ballot_requested);
}

void msgpack_pack_epoch_ballot_promise(msgpack_packer* packer, struct epoch_ballot_promise* promise){
    msgpack_pack_array(packer, 8);
    msgpack_pack_uint32(packer, WRITEAHED_EPOCH_BALLOT_PROMISE);
    msgpack_pack_uint32(packer, promise->acceptor_id);
    msgpack_pack_uint32(packer, promise->instance);
    msgpack_pack_epoch_ballot(packer, &promise->promised_epoch_ballot);
    msgpack_pack_epoch_ballot(packer, &promise->last_accepted_ballot);
    msgpack_pack_paxos_value(packer, &promise->last_accepted_value);
}

void msgpack_pack_epoch_ballot_accept(msgpack_packer* packer, struct epoch_ballot_accept* accept){
    msgpack_pack_array(packer, 5);
    msgpack_pack_uint32(packer, WRITEAHEAD_EPOCH_BALLOT_ACCEPT);
    msgpack_pack_uint32(packer, accept->instance);
    msgpack_pack_epoch_ballot(packer, &accept->epoch_ballot_requested);
    msgpack_pack_paxos_value(packer, &accept->value_to_accept);
}

void msgpack_pack_epoch_ballot_accepted(msgpack_packer* packer, struct epoch_ballot_accepted* accepted){
    msgpack_pack_array(packer, 6);
    msgpack_pack_uint32(packer, WRITEAHEAD_EPOCH_BALLOT_ACCEPTED);
    msgpack_pack_uint32(packer, accepted->acceptor_id);
    msgpack_pack_uint32(packer, accepted->instance);
    msgpack_pack_epoch_ballot(packer, &accepted->accepted_epoch_ballot);
    msgpack_pack_paxos_value(packer, &accepted->accepted_value);
}

void msgpack_pack_epoch_ballot_preempted(msgpack_packer* packer, struct epoch_ballot_preempted* preempted) {
    msgpack_pack_array(packer, 7);
    msgpack_pack_uint32(packer, WRITEAHEAD_EPOCH_BALLOT_PREEMPTED);
    msgpack_pack_uint32(packer, preempted->acceptor_id);
    msgpack_pack_uint32(packer, preempted->instance);
    msgpack_pack_epoch_ballot(packer, &preempted->requested_epoch_ballot);
    msgpack_pack_epoch_ballot(packer, &preempted->acceptors_current_epoch_ballot);
}


void msgpack_pack_instance_chosen_at_epoch_ballot(msgpack_packer* packer, struct instance_chosen_at_epoch_ballot* instance_chosen) {
    msgpack_pack_array(packer, 5);
    msgpack_pack_uint32(packer, WRITEAHEAD_INSTANCE_CHOSEN_AT_EPOCH_BALLOT);
    msgpack_pack_uint32(packer, instance_chosen->instance);
    msgpack_pack_epoch_ballot(packer, &instance_chosen->chosen_epoch_ballot);
    msgpack_pack_paxos_value(packer, &instance_chosen->chosen_value);
}



void msgpack_pack_epoch_notification(msgpack_packer* packer, struct epoch_notification* epoch_notification){
    msgpack_pack_array(packer, 2);
    msgpack_pack_uint32(packer, WRITEAHEAD_EPOCH_NOTIFICATION);
    msgpack_pack_uint32(packer, epoch_notification->new_epoch);
}

void msgpack_pack_writeahead_epoch_acceptor_state(msgpack_packer* packer, struct writeahead_epoch_acceptor_state* state){
    msgpack_pack_array(packer, 4);
    msgpack_pack_uint32(packer, WRITEAHEAD_ACCEPTOR_STATE);
    msgpack_pack_uint32(packer, state->standard_acceptor_state.aid);
    msgpack_pack_uint32(packer, state->standard_acceptor_state.trim_iid);
    msgpack_pack_uint32(packer, state->current_epoch);

}

void msgpack_pack_writeahead_epoch_paxos_message(msgpack_packer* packer, struct writeahead_epoch_paxos_message* message){
   switch (message->type) {
       case WRITEAHEAD_STANDARD_PREPARE:
           msgpack_pack_paxos_prepare(packer, &message->message_contents.standard_prepare);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_PREPARE:
           msgpack_pack_epoch_ballot_prepare(packer, &message->message_contents.epoch_ballot_prepare);
       case WRITEAHED_EPOCH_BALLOT_PROMISE:
           msgpack_pack_epoch_ballot_promise(packer, &message->message_contents.epoch_ballot_promise);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_ACCEPT:
            msgpack_pack_epoch_ballot_accept(packer, &message->message_contents.epoch_ballot_accept);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_ACCEPTED:
           msgpack_pack_epoch_ballot_accepted(packer, &message->message_contents.epoch_ballot_accepted);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_PREEMPTED:
           msgpack_pack_epoch_ballot_preempted(packer, &message->message_contents.epoch_ballot_preempted);
           break;
       case WRITEAHEAD_INSTANCE_CHOSEN_AT_EPOCH_BALLOT:
           msgpack_pack_instance_chosen_at_epoch_ballot(packer, &message->message_contents.instance_chosen_at_epoch_ballot);
           break;
       case WRITEAHEAD_EPOCH_NOTIFICATION:
           msgpack_pack_epoch_notification(packer, &message->message_contents.epoch_notification);
           break;
       case WRITEAHEAD_REPEAT:
           msgpack_pack_paxos_repeat(packer, &message->message_contents.repeat);
           break;
       case WRITEAHEAD_INSTANCE_TRIM:
           msgpack_pack_paxos_trim(packer, &message->message_contents.trim);
           break;
       case WRITEAHEAD_ACCEPTOR_STATE:
           msgpack_pack_writeahead_epoch_acceptor_state(packer, &message->message_contents.state);
           break;
       case WRITEAHEAD_CLIENT_VALUE:
           msgpack_pack_paxos_client_value(packer, &message->message_contents.client_value);
           break;
   }
}

static void msgpack_unpack_epoch_ballot(msgpack_object* msg_object, struct epoch_ballot* epoch_ballot_unpacked, int* i) {
    msgpack_unpack_uint32_at(msg_object, &epoch_ballot_unpacked->epoch, i);
    msgpack_unpack_ballot(msg_object, &epoch_ballot_unpacked->ballot, i);
}

void msgpack_unpack_epoch_ballot_prepare(msgpack_object* msg_object, struct epoch_ballot_prepare* prepare) {
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &prepare->instance, &i);
    msgpack_unpack_epoch_ballot(msg_object, &prepare->epoch_ballot_requested, &i);
}

void msgpack_unpack_epoch_ballot_promise(msgpack_object* msg_object, struct epoch_ballot_promise* unpacked_promise){
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &unpacked_promise->acceptor_id, &i);
    msgpack_unpack_uint32_at(msg_object, &unpacked_promise->instance, &i);
    msgpack_unpack_epoch_ballot(msg_object, &unpacked_promise->promised_epoch_ballot, &i);
    msgpack_unpack_epoch_ballot(msg_object, &unpacked_promise->last_accepted_ballot, &i);
    msgpack_unpack_paxos_value_at(msg_object, &unpacked_promise->last_accepted_value, &i);
}

void msgpack_unpack_epoch_ballot_accept(msgpack_object* msg_object, struct epoch_ballot_accept* unpacked_accept){
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &unpacked_accept->instance, &i);
    msgpack_unpack_epoch_ballot(msg_object, &unpacked_accept->epoch_ballot_requested, &i);
    msgpack_unpack_paxos_value_at(msg_object, &unpacked_accept->value_to_accept, &i);
}

void msgpack_unpack_epoch_ballot_accepted(msgpack_object* msg_object, struct epoch_ballot_accepted* unpacked_accepted) {
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &unpacked_accepted->acceptor_id, &i);
    msgpack_unpack_uint32_at(msg_object, &unpacked_accepted->instance, &i);
    msgpack_unpack_epoch_ballot(msg_object, &unpacked_accepted->accepted_epoch_ballot, &i);
    msgpack_unpack_paxos_value_at(msg_object, &unpacked_accepted->accepted_value, &i);
}

void msgpack_unpack_epoch_ballot_preempted(msgpack_object* msg_object, struct epoch_ballot_preempted* unpacked_preempted){
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &unpacked_preempted->acceptor_id, &i);
    msgpack_unpack_uint32_at(msg_object, &unpacked_preempted->instance, &i);
    msgpack_unpack_epoch_ballot(msg_object, &unpacked_preempted->requested_epoch_ballot, &i);
    msgpack_unpack_epoch_ballot(msg_object, &unpacked_preempted->acceptors_current_epoch_ballot, &i);
}

void msgpack_unpack_instance_chosen_at_epoch_ballot(msgpack_object* msg_object, struct instance_chosen_at_epoch_ballot* unpacked_instance_chosen){
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &unpacked_instance_chosen->instance, &i);
    msgpack_unpack_epoch_ballot(msg_object, &unpacked_instance_chosen->chosen_epoch_ballot, &i);
    msgpack_unpack_paxos_value_at(msg_object, &unpacked_instance_chosen->chosen_value, &i);
}



void msgpack_unpack_epoch_notification(msgpack_object* msg_object, struct epoch_notification* epoch_notification){
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &epoch_notification->new_epoch, &i);
}


void msgpack_unpack_writeahead_epoch_acceptor_state(msgpack_object* msg_object, struct writeahead_epoch_acceptor_state* state){
    int i = 0;
    msgpack_unpack_uint32_at(msg_object, &state->standard_acceptor_state.aid, &i);
    msgpack_unpack_uint32_at(msg_object, &state->standard_acceptor_state.trim_iid, &i);
    msgpack_unpack_uint32_at(msg_object, &state->current_epoch, &i);
}

void msgpack_unpack_writeahead_epoch_paxos_message(msgpack_object* msg_object, struct writeahead_epoch_paxos_message* unpacked_message){
   unpacked_message->type = MSGPACK_OBJECT_AT(msg_object,0).u64;

   switch (unpacked_message->type) {

       case WRITEAHEAD_STANDARD_PREPARE:
           msgpack_unpack_paxos_prepare(msg_object, &unpacked_message->message_contents.standard_prepare);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_PREPARE:
           msgpack_unpack_epoch_ballot_prepare(msg_object, &unpacked_message->message_contents.epoch_ballot_prepare);
           break;
       case WRITEAHED_EPOCH_BALLOT_PROMISE:
           msgpack_unpack_epoch_ballot_promise(msg_object, &unpacked_message->message_contents.epoch_ballot_promise);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_ACCEPT:
           msgpack_unpack_epoch_ballot_accept(msg_object, &unpacked_message->message_contents.epoch_ballot_accept);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_ACCEPTED:
           msgpack_unpack_epoch_ballot_accepted(msg_object, &unpacked_message->message_contents.epoch_ballot_accepted);
           break;
       case WRITEAHEAD_EPOCH_BALLOT_PREEMPTED:
           msgpack_unpack_epoch_ballot_preempted(msg_object, &unpacked_message->message_contents.epoch_ballot_preempted);
           break;
       case WRITEAHEAD_EPOCH_NOTIFICATION:
           msgpack_unpack_epoch_notification(msg_object, &unpacked_message->message_contents.epoch_notification);
           break;
       case WRITEAHEAD_INSTANCE_CHOSEN_AT_EPOCH_BALLOT:
           msgpack_unpack_instance_chosen_at_epoch_ballot(msg_object, &unpacked_message->message_contents.instance_chosen_at_epoch_ballot);
           break;
       case WRITEAHEAD_REPEAT:
           msgpack_unpack_paxos_repeat(msg_object, &unpacked_message->message_contents.repeat);
           break;
       case WRITEAHEAD_INSTANCE_TRIM:
           msgpack_unpack_paxos_trim(msg_object, &unpacked_message->message_contents.trim);
           break;
       case WRITEAHEAD_ACCEPTOR_STATE:
           msgpack_unpack_writeahead_epoch_acceptor_state(msg_object, &unpacked_message->message_contents.state);
           break;
       case WRITEAHEAD_CLIENT_VALUE:
           msgpack_unpack_paxos_client_value(msg_object, &unpacked_message->message_contents.client_value);
           break;
   }
}


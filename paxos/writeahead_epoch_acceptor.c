//
// Created by Michael Davis on 04/02/2020.
//

#include <paxos.h>
#include <epoch_stable_storage.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <epoch_paxos_storage.h>
#include <paxos_types.h>
#include <paxos_message_conversion.h>
#include <stdbool.h>

struct writeahead_epoch_acceptor {
    int id;
    struct epoch_stable_storage stable_storage;
    struct epoch_paxos_storage volatile_storage;
    iid_t trim_instance; // here for fast access
    uint32_t current_epoch;
};

void writeahead_epoch_acceptor_increase_epoch(struct writeahead_epoch_acceptor* acceptor, uint32_t new_epoch){
    // stored epochs should only ever increase
    assert(new_epoch >= acceptor->current_epoch);
    acceptor->current_epoch = new_epoch;
    epoch_stable_storage_store_epoch(&acceptor->stable_storage, new_epoch);
}

void writeahead_epoch_acceptor_store_trim(struct writeahead_epoch_acceptor* acceptor, iid_t trim) {
    acceptor->trim_instance = trim;
    epoch_stable_storage_store_trim_instance(&acceptor->stable_storage, trim);
}

bool writeahead_epoch_acceptor_epoch_ballot_greater_than_or_equal_to(struct epoch_ballot* left, struct epoch_ballot* right) {
    if (left->epoch > right->epoch) return true;
 //   else if (left->epoch == right->epoch && left->ballot >= right->ballot) return true;
    else return false;
}

void create_epoch_notification_message(struct writeahead_epoch_paxos_message *recover_message,
                                       const struct writeahead_epoch_acceptor *acceptor) {
    recover_message->type = WRITEAHEAD_EPOCH_NOTIFICATION;
    recover_message->message_contents.epoch_notification.new_epoch = acceptor->current_epoch;
}

struct writeahead_epoch_acceptor* writeahead_epoch_acceptor_init(int id, struct writeahead_epoch_paxos_message* recover_message) {
    struct writeahead_epoch_acceptor* acceptor = calloc(1, sizeof(struct writeahead_epoch_acceptor));
    acceptor->id = id;
    epoch_stable_storage_init(&acceptor->stable_storage, id);

    if (epoch_stable_storage_open(&acceptor->stable_storage) != 0) {
        free(acceptor);
        return NULL;
    }

    epoch_stable_storage_tx_begin(&acceptor->stable_storage);

    // Increment epoch
    uint32_t old_epoch = 0;
    int was_previous_epoch = epoch_stable_storage_get_current_epoch(&acceptor->stable_storage, &old_epoch);
    if (was_previous_epoch != 0) {
        uint32_t recovery_epoch = old_epoch + 1;
        writeahead_epoch_acceptor_increase_epoch(acceptor, recovery_epoch);
    } else {
        writeahead_epoch_acceptor_increase_epoch(acceptor, 0);
    }



    // Recover all accepts from stable storage and copy them to the volatile storage (for faster access and no disk writes during promising)
    epoch_stable_storage_get_trim_instance(&acceptor->stable_storage, &acceptor->trim_instance);
    int number_of_accepts = 0;
    struct epoch_ballot_accept* all_accepted_epoch_ballot_accepts;
    epoch_stable_storage_get_all_untrimmed_epoch_ballot_accepts(&acceptor->stable_storage, &all_accepted_epoch_ballot_accepts, &number_of_accepts);

    epoch_paxos_storage_init_with_prepares_and_accepts(&acceptor->volatile_storage, NULL, 0, &all_accepted_epoch_ballot_accepts, number_of_accepts); // TODO check if old accepteds the min promise for the epoch is better

    iid_t trim = 0;
    epoch_stable_storage_get_trim_instance(&acceptor->stable_storage, &trim);
    writeahead_epoch_acceptor_store_trim(acceptor, trim);


    // Storage not used past here, so can commit tx
    epoch_stable_storage_tx_commit(&acceptor->stable_storage);

    // double check epoch was incremented correctly & volatile storage matches stable
    uint32_t incremented_epoch = 0;
    epoch_stable_storage_tx_begin(&acceptor->stable_storage);
    epoch_stable_storage_get_current_epoch(&acceptor->stable_storage, &incremented_epoch);

    uint32_t max_instance = 0;
    epoch_stable_storage_get_max_inited_instance(&acceptor->stable_storage, &max_instance);
    for (unsigned int i =0; i <= max_instance; i++){
        // todo
    }

    epoch_stable_storage_tx_commit(&acceptor->stable_storage);

    assert(incremented_epoch > old_epoch);


    if (acceptor->current_epoch > 0) {
        create_epoch_notification_message(recover_message, acceptor);
    }

    return acceptor;
}

void writeahead_epoch_acceptor_free(struct writeahead_epoch_acceptor* acceptor){
    epoch_stable_storage_close(&acceptor->stable_storage);
    free(acceptor);
}

int writeahead_epoch_acceptor_receive_prepare(struct writeahead_epoch_acceptor* acceptor, struct paxos_prepare* request, struct writeahead_epoch_paxos_message* returned_message){
    int is_a_message_returned = 0;

    bool instance_chosen = false;
    epoch_paxos_storage_is_instance_chosen(&acceptor->volatile_storage, request->iid, &instance_chosen);
    if (request->iid > acceptor->trim_instance && instance_chosen) {

        // Compare to instance's last prepare made
        struct paxos_prepare last_prepare;
        memset(&last_prepare, 0, sizeof(struct paxos_prepare));
        epoch_paxos_storage_get_last_prepare(&acceptor->volatile_storage, request->iid, &last_prepare); // todo check if it is better toalso consider the epoch

     //  if (request->ballot >= last_prepare.ballot) {
           // We are going to return an epoch ballot promise for this instance on the
           // requested ballot, providing our last (highest) accepted epoch ballot and value
            paxos_log_debug("New highest ballot (%u) for instance %u", request->ballot, request->iid);

            // Store the prepare (in volatile storage)
           epoch_paxos_storage_store_last_prepare(&acceptor->volatile_storage, request);

           // get the last accepted ballot from stable storage
           struct epoch_ballot_accept last_accept;
           memset(&last_accept, 0, sizeof(last_accept));
           epoch_paxos_storage_get_last_accept(&acceptor->volatile_storage, request->iid, &last_accept);

           // create the returned promise
           union_epoch_ballot_promise_from_epoch_ballot_accept_and_paxos_prepare(returned_message, request, &last_accept, acceptor->id, acceptor->current_epoch);
           is_a_message_returned = 1;
       } else {
           // return preempted
            struct epoch_ballot_preempted preempted;

            //struct epoch_ballot reqested_eb = (struct epoch_ballot) {.epoch = acceptor->current_epoch, .ballot = request->ballot};
       //     struct epoch_ballot last_eb = (struct epoch_ballot) {.epoch = acceptor->current_epoch, .ballot =last_prepare.ballot};

      //     epoch_ballot_preempted_from_epoch_ballot_requested_and_epoch_ballot_last_responded(acceptor->id, request->iid, &reqested_eb, &last_eb, &preempted);
           union_epoch_ballot_preempted_from_epoch_ballot_preempted(&preempted, returned_message);
           is_a_message_returned = 1;
       }
   // } else {
        //return chosen
        struct epoch_ballot_accept last_accept;
        epoch_paxos_storage_get_last_accept(&acceptor->volatile_storage, request->iid, &last_accept);

        union_epoch_ballot_chosen_from_epoch_ballot_accept(returned_message, &last_accept);
        is_a_message_returned = 1;
//    }
    return is_a_message_returned;
}

int writeahead_epoch_acceptor_receive_epoch_ballot_prepare(struct writeahead_epoch_acceptor* acceptor, struct epoch_ballot_prepare* request, struct writeahead_epoch_paxos_message* returned_message){
    // Check if instance has been trimmed
    int is_a_message_returned = 0;

    bool instance_chosen = false;
    epoch_paxos_storage_is_instance_chosen(&acceptor->volatile_storage, request->instance, &instance_chosen);
    if (request->instance > acceptor->trim_instance && instance_chosen) {

        // Compare to instance's last prepare made
        struct paxos_prepare last_prepare;
        memset(&last_prepare, 0, sizeof(struct paxos_prepare));
        epoch_paxos_storage_get_last_prepare(&acceptor->volatile_storage, request->instance, &last_prepare);

        struct epoch_ballot current_epoch_ballot = (struct epoch_ballot) {.epoch = acceptor->current_epoch, .ballot = last_prepare.ballot}; // todo check if it is better to include the actual promised epoch ballot - shouldn't cause if it were the current ballot on a new epoch then it will get promised still and we don't really wanna worry about promising old ballots on new epochs
        if (writeahead_epoch_acceptor_epoch_ballot_greater_than_or_equal_to(&request->epoch_ballot_requested, &current_epoch_ballot)) {

            if (request->epoch_ballot_requested.epoch > acceptor->current_epoch) {
                epoch_stable_storage_tx_begin(&acceptor->stable_storage);
                writeahead_epoch_acceptor_increase_epoch(acceptor, request->epoch_ballot_requested.epoch);
                epoch_stable_storage_tx_commit(&acceptor->stable_storage);
            }
            // We are going to return an epoch ballot promise for this instance on the
            // requested ballot, providing our last (highest) accepted epoch ballot and value
            paxos_log_debug("New highest ballot (%u, %u) for instance %u", request->epoch_ballot_requested.epoch, request->epoch_ballot_requested.ballot, request->instance);

            // Store the prepare (in volatile storage)
            struct paxos_prepare stored_prepare;
            //struct paxos_prepare stored_prepare = {.ballot = request->epoch_ballot_requested.ballot, .iid = request->instance};
            paxos_prepare_from_epoch_ballot_prepare(request, &stored_prepare);
            epoch_paxos_storage_store_last_prepare(&acceptor->volatile_storage, &stored_prepare);

            // get the last accepted ballot from stable storage
            struct epoch_ballot_accept last_accept;
            memset(&last_accept, 0, sizeof(last_accept));
            epoch_paxos_storage_get_last_accept(&acceptor->volatile_storage, request->instance, &last_accept);

            // create the returned promise
            union_epoch_ballot_promise_from_epoch_ballot_accept_and_epoch_ballot_prepare(returned_message, request, &last_accept, acceptor->id);
            is_a_message_returned = 1;
        } else {
            // return preempted
            struct epoch_ballot_preempted preempted;

            struct epoch_ballot reqested_eb = (struct epoch_ballot) {.epoch = request->epoch_ballot_requested.epoch, .ballot = request->epoch_ballot_requested.ballot};
            struct epoch_ballot last_eb = (struct epoch_ballot) {.epoch = acceptor->current_epoch, .ballot =last_prepare.ballot};

            epoch_ballot_preempted_from_epoch_ballot_requested_and_epoch_ballot_last_responded(acceptor->id, request->instance, &reqested_eb, &last_eb, &preempted);
            union_epoch_ballot_preempted_from_epoch_ballot_preempted(&preempted, returned_message);
            is_a_message_returned = 1;
        }
    } else {
        // return chosen
        struct epoch_ballot_accept last_accept;
        epoch_paxos_storage_get_last_accept(&acceptor->volatile_storage, request->instance, &last_accept);

        union_epoch_ballot_chosen_from_epoch_ballot_accept(returned_message, &last_accept);
        is_a_message_returned = 1;
    }
    return is_a_message_returned;
}

void writeahead_epoch_acceptor_store_accept(struct writeahead_epoch_acceptor* acceptor, struct epoch_ballot_accept* accept) {
    assert(acceptor->current_epoch >= accept->epoch_ballot_requested.epoch);
    acceptor->current_epoch = accept->epoch_ballot_requested.epoch;
    epoch_stable_storage_store_epoch_ballot_accept(&acceptor->stable_storage, accept);
}

int writeahead_epoch_acceptor_receive_epoch_ballot_accept(struct writeahead_epoch_acceptor* acceptor, struct epoch_ballot_accept* request, struct writeahead_epoch_paxos_message* response) {
    // Check if instance has been trimmed
    int is_a_message_returned = 0;

    bool instance_chosen = false;
    epoch_paxos_storage_is_instance_chosen(&acceptor->volatile_storage, request->instance, &instance_chosen);
    if (request->instance > acceptor->trim_instance && instance_chosen) {

        // Compare to instance's last prepare made
        struct paxos_prepare last_prepare;
        memset(&last_prepare, 0, sizeof(struct paxos_prepare));
        epoch_paxos_storage_get_last_prepare(&acceptor->volatile_storage, request->instance, &last_prepare);

        struct epoch_ballot current_epoch_ballot = (struct epoch_ballot) {.epoch = acceptor->current_epoch, .ballot = last_prepare.ballot};
        // todo check if it is better to include the actual promised epoch ballot - shouldn't cause if it were the current ballot on a new epoch then it will get promised still and we don't really wanna worry about promising old ballots on new epochs
        if (writeahead_epoch_acceptor_epoch_ballot_greater_than_or_equal_to(&request->epoch_ballot_requested, &current_epoch_ballot)) {


            // We are going to return an epoch ballot promise for this instance on the
            // requested ballot, providing our last (highest) accepted epoch ballot and value
            paxos_log_debug("Accepted highest ballot (%u, %u) for instance %u", request->epoch_ballot_requested.epoch, request->epoch_ballot_requested.ballot, request->instance);

            // Store the accept (in stable!!!!! storage)
            epoch_stable_storage_tx_begin(&acceptor->stable_storage);
            writeahead_epoch_acceptor_store_accept(acceptor, request); // don't need to store epoch
            epoch_stable_storage_tx_commit(&acceptor->stable_storage);

            // get the last accepted ballot from stable storage
            struct epoch_ballot_accept last_accept;
            memset(&last_accept, 0, sizeof(last_accept));
            epoch_paxos_storage_get_last_accept(&acceptor->volatile_storage, request->instance, &last_accept);

            // create the returned accepted

            //todo accepted
            union_epoch_ballot_accepted_from_epoch_ballot_accept(response, request, acceptor->id);
            is_a_message_returned = 1;
        } else {
            // return preempted
            struct epoch_ballot_preempted preempted;

            struct epoch_ballot reqested_eb = (struct epoch_ballot) {.epoch = request->epoch_ballot_requested.epoch, .ballot = request->epoch_ballot_requested.ballot};
            struct epoch_ballot last_eb = (struct epoch_ballot) {.epoch = acceptor->current_epoch, .ballot =last_prepare.ballot};

            epoch_ballot_preempted_from_epoch_ballot_requested_and_epoch_ballot_last_responded(acceptor->id, request->instance, &reqested_eb, &last_eb, &preempted);
            union_epoch_ballot_preempted_from_epoch_ballot_preempted(&preempted, response);
            is_a_message_returned = 1;
        }
    } else {
        struct epoch_ballot_accept last_accept;
        epoch_paxos_storage_get_last_accept(&acceptor->volatile_storage, request->instance, &last_accept);

        union_epoch_ballot_chosen_from_epoch_ballot_accept(response, &last_accept);
        is_a_message_returned = 1;
    }
    return is_a_message_returned;
}

int  writeahead_epoch_acceptor_receive_repeat(struct writeahead_epoch_acceptor* acceptor, iid_t iid, struct writeahead_epoch_paxos_message* response); //todo

int  writeahead_epoch_acceptor_receive_trim(struct writeahead_epoch_acceptor* acceptor, struct paxos_trim* trim); //todo

int  writeahead_epoch_acceptor_receive_epoch_notification(struct writeahead_epoch_acceptor* acceptor, struct epoch_notification* epoch_notification){
   if (epoch_notification->new_epoch > acceptor->current_epoch) {
      writeahead_epoch_acceptor_increase_epoch(acceptor, epoch_notification->new_epoch);
   }
   return 0; // no message to send
}

int writeahead_epoch_acceptor_receive_instance_chosen(struct writeahead_epoch_acceptor* acceptor, struct instance_chosen_at_epoch_ballot chosen_message){
    epoch_paxos_storage_set_instance_chosen(&acceptor->volatile_storage, chosen_message.instance);
    return 1;
}


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

#include "write_ahead_window_acceptor.h"
#include "standard_acceptor.h"

#include "stable_storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <paxos_message_conversion.h>
#include <paxos_storage.h>
#include <paxos_types.h>
#include <hash_mapped_memory.h>


struct write_ahead_window_acceptor {
    // Easy to just extend off of the standard acceptor
    struct standard_acceptor* standard_acceptor;

    // Volatile storage to store the "actual" promises made to proposers
    // Will be lost on restart
    // Important to ensure that any promise given to a proposer is written ahead to the
    // standard acceptor's stable storage
    struct paxos_storage* volatile_storage;

    // A stable storage duplicate is included to allow the Acceptor to handle responding to
    // Promise Requests and Accept Requests without ever having to being a transaction in Stable Storage
    // Of course this only applies when an Acceptor doesn't have to safely storage anything
    // Need to ensure that all promises and acceptances written to stable storage are written
    // both the the standard acceptor's and the duplicate
    struct paxos_storage* stable_storage_duplicate; //  TODO add in to speed up access

    // The minimum point in which volatile storage is allowed to catch up with stable storage
    int min_ballot_catachup;
    int min_instance_catachup;

    // The number of ballots or instances that should be written ahead of the last stable storage point
    int ballot_window;
    int instance_window;

    // Here is the info necessary to determine whether or not to adjust the ballot window of the last updated instance
    // The boolean determines if it does
    // The int determines the instance to update
    // These variables are set when recieving a ballot from a particular instance
    // By flagging during that point, the ballot for the instance can be written ahead after responding to the message
    bool last_instance_needs_new_ballot_epoch;
    int last_instance; // todo change from loop to just one
};


void write_ahead_window_acceptor_store_in_stable_storage(struct write_ahead_window_acceptor* acceptor,
                                                  const struct paxos_accepted* instance_info){
    storage_store_instance_info(&acceptor->standard_acceptor->stable_storage, instance_info);
    store_instance_info(acceptor->stable_storage_duplicate, instance_info);
}

// Stores to all types of storage of the Acceptor the instance info.
// For stable storage this will be uncommitted, so the user of the method must commit the stable storage's
// transcation themselves
void
write_ahead_window_acceptor_store_in_all_storages(struct write_ahead_window_acceptor* acceptor,
                                           const struct paxos_accepted* instance_info)
{
    write_ahead_window_acceptor_store_in_stable_storage(acceptor, instance_info);
    store_instance_info(acceptor->volatile_storage, instance_info);
}


void write_ahead_window_acceptor_store_prepare_in_volatile_storage(struct write_ahead_window_acceptor* acceptor, const struct paxos_prepare* prepare){
    store_last_prepare(acceptor->volatile_storage, prepare);
}

// When we want to write a head a window of promises from a specific ballot
void
write_ahead_window_acceptor_new_ballot_epoch_from_ballot(struct write_ahead_window_acceptor* acceptor,
                                                  const iid_t instance,
                                                  const uint32_t ballot) {
    // todo error checking and reting from method
    // Initialising memory for the instance info variable
    struct paxos_accepted instance_info;//calloc(1, sizeof(struct paxos_accepted));
    memset(&instance_info, 0, sizeof(struct paxos_accepted));
    instance_info.iid = instance;
    //memset(&instance_info, 0, sizeof(struct paxos_accepted));

    // Get the instance info from stable storage
    storage_get_instance_info(&acceptor->standard_acceptor->stable_storage, instance, &instance_info);     // get the acceptor info for that instance
    instance_info.ballot = ballot + acceptor->ballot_window;    // write a new accepted message for that instance with the write ahead promise

    // Store new instance info to stable storage
    write_ahead_window_acceptor_store_in_stable_storage(acceptor, &instance_info);
    paxos_log_debug("Writing ahead new ballot epoch to instance %u; new ballot: %u", instance_info.iid, instance_info.ballot);
}


// Write a promise ahead from the previous stably stored ballot
// If the instance has never been initialised, then the Acceptor
// simply writes ahead a promise the size of the ballot_window
void write_ahead_window_acceptor_new_ballot_epoch_from_last_epoch(struct write_ahead_window_acceptor* acceptor,
                                                           iid_t instance){
    int error = -1;
    // todo error checking and returing from method

    // Initialising memory for the instance info variable
    struct paxos_accepted instance_info;//= calloc(1, sizeof(struct paxos_accepted));
    memset(&instance_info, 0, sizeof(struct paxos_accepted));
    instance_info.iid = instance;
    // Get the instance info from stable storage and update it with the new window
    int found = storage_get_instance_info(&acceptor->standard_acceptor->stable_storage, instance, &instance_info);     // get the acceptor info for that instnace
    instance_info.ballot = instance_info.ballot + acceptor->ballot_window;    // write a new accepted message for that instance with the write ahead promise

    // Store new instance info to stable storage
    write_ahead_window_acceptor_store_in_stable_storage(acceptor, &instance_info);

    paxos_log_info("Instance: %u. Written ahead new epoch at ballot %u", instance_info.iid, instance_info.ballot);
    // Cleanup
    //ree(instance_info);
}


// Check whether it a ballot has been safely prewritten to stable storage
// This is important to ensure it is safe to give a promise on a ballot
bool
write_ahead_window_acceptor_is_instance_written_ahead(struct write_ahead_window_acceptor* acceptor,
                                               iid_t instance,
                                               uint32_t ballot)
{
    iid_t *last_inited_instance = calloc(1, sizeof(iid_t));
    storage_get_max_inited_instance(&acceptor->standard_acceptor->stable_storage, last_inited_instance);

    if (*last_inited_instance < instance) {
        free(last_inited_instance);
        paxos_accepted* stable_instance_info = calloc(1, sizeof(struct paxos_accepted));
        storage_get_instance_info(&acceptor->standard_acceptor->stable_storage, instance, stable_instance_info);

        if (stable_instance_info->ballot > ballot) {
            free(stable_instance_info);
            return true;
        } else {
            free(stable_instance_info);
            return false;
        }
    } else {
        free(last_inited_instance);
        return false;
    }
}




/*
 *  WRITING AHEAD PROMISES AND INSTANCES
 */

// write ahead promises for all instances past trim until the end of the instance window of unititiated instnaces
// to be used after recovery
void
write_ahead_window_acceptor_new_ballots_and_instance_epochs(struct write_ahead_window_acceptor* acceptor) {
    int error;
    iid_t  max_inited_instance ;//b= calloc(1, sizeof(iid_t));
    error = storage_tx_begin(&acceptor->standard_acceptor->stable_storage);
    error = storage_get_max_inited_instance(&acceptor->standard_acceptor->stable_storage, &max_inited_instance);

    // for all those instances between the trim and the instance window write ahead promises
    // Dont use write_ahead_window_acceptor_new_instance_epoch() here because we would have to get the max_inited_instance from storage again
    iid_t i = acceptor->standard_acceptor->trim_iid;
    while (i <= (max_inited_instance + acceptor->instance_window)){
        write_ahead_window_acceptor_new_ballot_epoch_from_last_epoch(acceptor, i);
        i++;
    }

    error = storage_tx_commit(&acceptor->standard_acceptor->stable_storage);
    // work out whther or not to delete
}


// write ahead promises for unititinated instances
// a
void
write_ahead_window_acceptor_new_instance_epoch(struct write_ahead_window_acceptor* acceptor) {
    iid_t max_inited_instance ;//= calloc(1, sizeof(iid_t));
    storage_get_max_inited_instance(&acceptor->standard_acceptor->stable_storage, &max_inited_instance);

    iid_t new_instance_epoch_end = max_inited_instance + acceptor->instance_window;

    // for all those instances between the trim and the instance window write ahead promises
    for (iid_t i = max_inited_instance + 1; i <= new_instance_epoch_end; i++) {
        write_ahead_window_acceptor_new_ballot_epoch_from_last_epoch(acceptor, i);
    }

    paxos_log_debug("Written ahead Epoch. Max Inited Instance now %u", new_instance_epoch_end);
}


// has the ballot window gotten too small?
bool
write_ahead_window_acceptor_does_instance_ballot_window_need_adjusting(uint32_t written_ahead_ballot,
                                                                uint32_t min_ballot_window_size,
                                                                uint32_t last_volatile_ballot)
{
    if ((written_ahead_ballot - last_volatile_ballot) < min_ballot_window_size) {
        return true;
    } else {
        return false;
    }
}


// Checks to see if the instance window needs adjusting
bool
write_ahead_window_acceptor_does_instance_window_need_adjusting(iid_t max_inited_instance,
                                                         iid_t last_instance_before_prev_epoch,
                                                         int min_window_size)
{
    if ((max_inited_instance - last_instance_before_prev_epoch) < min_window_size) {
        return true;
    } else {
        return false;
    }
}


void check_and_update_last_updated_instance_ballot_window(struct write_ahead_window_acceptor *acceptor) {
    if(acceptor->last_instance_needs_new_ballot_epoch){

        struct paxos_prepare volatile_promise;
        get_last_promise(acceptor->volatile_storage, acceptor->last_instance, &volatile_promise);

        // if the difference between the stably held ballot and the volatile ballot is less than the min window size
        write_ahead_window_acceptor_new_ballot_epoch_from_ballot(acceptor, acceptor->last_instance,
                                                          volatile_promise.ballot); // write ahead a window from the last volatile ballot

        // cleanup - reset
        acceptor->last_instance = -1;

        acceptor->last_instance_needs_new_ballot_epoch = false;
    }
}


void check_and_update_instance_window(struct write_ahead_window_acceptor *acceptor, const iid_t max_inited_instance) {
    if (write_ahead_window_acceptor_does_instance_window_need_adjusting(max_inited_instance,
                                                                 acceptor->last_instance,
                                                                 acceptor->min_instance_catachup)){
        paxos_log_debug("Instance Window has caught up\n"
                        "Writing ahead new Instance Epoch");
        write_ahead_window_acceptor_new_instance_epoch(acceptor);
    }
}


// Meant to be a method to check the windows (ballot and instance) after replying to a proposer message
// could be async but not really supported
void
write_ahead_window_acceptor_check_and_update_write_ahead_windows(struct write_ahead_window_acceptor* acceptor) {
    // TODO add error checking and handling
    // for each instance if the ballot last promised ballot in memory is too close to the one in stable storage then write ahead

    // for the write ahead instance
    // if the max init instance is near the the write ahead point then write initiate those instances
    check_and_update_last_updated_instance_ballot_window(acceptor);

    // Get the maximum instance id to be initialised (promised or accepted in)
    iid_t max_inited_instance; //= calloc(1, sizeof(iid_t));
    storage_tx_begin(&acceptor->standard_acceptor->stable_storage);
    storage_get_max_inited_instance(&acceptor->standard_acceptor->stable_storage, &max_inited_instance);
    check_and_update_instance_window(acceptor, max_inited_instance);
    storage_tx_commit(&acceptor->standard_acceptor->stable_storage);

   // free(max_inited_instance);
}



/*
 * RECOVERY, INITIALISATION, DEALLOCATION AND SETTERS
 */



// This method takes all those ballots
void
write_ahead_window_acceptor_copy_to_paxos_storage_from_stable_storage(struct stable_storage* stable_storage,
                                                               struct paxos_storage *store_to_copy_to,
                                                               iid_t from_instance) {
    // Todo add error handling
    int error;

    iid_t* max_ininted_instance = calloc(1, sizeof(iid_t));


    storage_tx_begin(stable_storage);
    error = storage_get_max_inited_instance(stable_storage, max_ininted_instance);


    for (int i = from_instance; i <= *max_ininted_instance; i++) {
        paxos_accepted *instance_info = NULL;
        //memset(instance_info, 0, sizeof(paxos_accepted));
        if ((error = storage_get_instance_info(stable_storage, i, instance_info)) == 0) {
            error = store_instance_info(store_to_copy_to, instance_info);
        }
    }

    storage_tx_commit(stable_storage);
    free(max_ininted_instance);
}



struct write_ahead_window_acceptor*
write_ahead_window_acceptor_new (
        int id,
        int min_instance_catchup,
        int min_ballot_catchup,
        int bal_window,
        int instance_window
            )
{
    int error;
    struct write_ahead_window_acceptor* write_ahead_window_acceptor =  calloc(1, sizeof(struct write_ahead_window_acceptor));

    //stable storage setup
    write_ahead_window_acceptor->standard_acceptor = standard_acceptor_new(id);//write_ahead_window_acceptor_init_standard_acceptor(id, stable_storage);


    // get instances info from stable storage
    error = storage_tx_begin(&write_ahead_window_acceptor->standard_acceptor->stable_storage);
    paxos_accepted* instances_info ;// calloc(1, sizeof(struct paxos_accepted));//calloc(0, sizeof(struct paxos_accepted)); // not initialised as it is unknown how many instances will be retrieved
    int* number_of_instances = calloc(1, sizeof(int));
    error = storage_get_all_untrimmed_instances_info(&write_ahead_window_acceptor->standard_acceptor->stable_storage, &instances_info, number_of_instances);

    // get trim id from stable storage
    iid_t* trim_id = calloc(1, sizeof(trim_id));
    error = storage_get_trim_instance(&write_ahead_window_acceptor->standard_acceptor->stable_storage, trim_id);

    error = storage_tx_commit(&write_ahead_window_acceptor->standard_acceptor->stable_storage);

    //copy to ss duplicate
    struct paxos_storage* stable_storage_duplicate = calloc(1, sizeof(struct paxos_storage));
    init_hash_mapped_memory_from_instances_info(stable_storage_duplicate, instances_info, *number_of_instances, *trim_id);

    //copy to volitile storage
    struct paxos_storage* volatile_storage = calloc(1, sizeof(struct paxos_storage));
    init_hash_mapped_memory_from_instances_info(volatile_storage, instances_info, *number_of_instances, *trim_id);


    // Set up write ahead acceptor variables
    write_ahead_window_acceptor->stable_storage_duplicate = stable_storage_duplicate;
    write_ahead_window_acceptor->volatile_storage = volatile_storage;
    write_ahead_window_acceptor->instance_window = instance_window;
    write_ahead_window_acceptor->ballot_window = bal_window;
    write_ahead_window_acceptor->min_instance_catachup = min_instance_catchup;
    write_ahead_window_acceptor->min_ballot_catachup = min_ballot_catchup;

    write_ahead_window_acceptor->last_instance_needs_new_ballot_epoch = false;
    write_ahead_window_acceptor->last_instance = -1;
    write_ahead_window_acceptor->last_instance = 0;
    // Some explaination about what is happening here:
    // When the Acceptor restarts it can assume that the last ballot held in stable storage is safe as it will have
    // never previously responded to a ballot higher.
    // So we can allow the acceptor to respond to ballots greater than or equal to that ballot.
    // We then Write ahead promises for previously initiated instances and
    // initiate some new instances with written ahead promises


    // Transfer all info from untrimmed instances to volatile storage so that it can returned faster

    // Write ahead promises and initiate new instances

    // todo free instances info
    write_ahead_window_acceptor_new_ballots_and_instance_epochs(write_ahead_window_acceptor);

    return write_ahead_window_acceptor;
}


void write_ahead_window_acceptor_check_and_flag_instance_for_new_ballot_epoch(struct write_ahead_window_acceptor *a,
                                                                       const paxos_prepare *req,
                                                                       const struct paxos_prepare *last_promise,
                                                                       const struct paxos_prepare *last_stable_promise);

void
write_ahead_window_acceptor_free(struct write_ahead_window_acceptor* a)
{
    storage_close(&a->standard_acceptor->stable_storage);
    free(a);
}


void
write_ahead_window_acceptor_set_current_state(
        struct write_ahead_window_acceptor* a,
        paxos_standard_acceptor_state* state
            )
{
    state->aid = a->standard_acceptor->id;
    state->trim_iid = a->standard_acceptor->trim_iid;
}


/*
 * HANDLING OF MESSAGES RECEIVED
 * -----------------------------
 */

// Acceptor handler for receiving a prepare message.
// If returned 0 then there is no message/response from the acceptor,
// otherwise, there is.
int
write_ahead_window_acceptor_receive_prepare(
        struct write_ahead_window_acceptor* a,
        paxos_prepare* req,
        paxos_message* out
            )
{
    bool is_there_response_message = false; // Initially we don't have a message to response with

    // If the Request is for an already trimmed instance we can just ignore it
    // We could maybe in future change this to be some sort of response to inform the Requestor
    if (req->iid <= a->standard_acceptor->trim_iid) {
        return is_there_response_message;
    }

    struct paxos_prepare last_volatile_promise ;//= calloc(1, sizeof(struct paxos_prepare));
    memset(&last_volatile_promise, 0, sizeof(struct paxos_prepare));
    int found = get_last_promise(a->volatile_storage, req->iid, &last_volatile_promise);

    // Check if ballot is higher than last promised ballot (or accepted ballot)
    if(!found || last_volatile_promise.ballot <= req->ballot) {
        // promise request is newest
        struct paxos_prepare last_stable_promise ;//= calloc(1, sizeof(struct paxos_prepare));
        memset(&last_stable_promise, 0, sizeof(struct paxos_prepare));
        get_last_promise(a->stable_storage_duplicate, req->iid, &last_stable_promise);

        write_ahead_window_acceptor_store_prepare_in_volatile_storage(a, req);

        // TODO check if should be here
        a->last_instance = req->iid;

        // Chwck is promise is written ahead
        if (last_stable_promise.ballot < req->ballot) {
            // Must write new requested ballot to stable storage
            if (storage_tx_begin(&a->standard_acceptor->stable_storage) != 0){
                return 0;
            }

            write_ahead_window_acceptor_new_ballot_epoch_from_ballot(a, req->iid, req->ballot);

            if (storage_tx_commit(&a->standard_acceptor->stable_storage) != 0) {
                return 0;
            }
        } else {

            // If we don't write ahead the promise now, we want to check if we want to adjust it
            // before the next Promise Request comes in
            write_ahead_window_acceptor_check_and_flag_instance_for_new_ballot_epoch(a, req, &last_volatile_promise,
                                                                              &last_stable_promise);
        }
        // now prepare the retured message (a promise)
        struct paxos_accept last_accept;//calloc(1, sizeof(struct paxos_accept));
        memset(&last_accept, 0, sizeof(struct paxos_accept));
        get_last_accept(a->volatile_storage, req->iid, &last_accept);

        paxos_promise_from_accept_and_prepare(req, &last_accept, a->standard_acceptor->id, out);
        is_there_response_message = true;
    } // TODO check if there needs to be preempted added here
    return is_there_response_message;
}

void write_ahead_window_acceptor_check_and_flag_instance_for_new_ballot_epoch(struct write_ahead_window_acceptor *a,
                                                                       const paxos_prepare *req,
                                                                       const struct paxos_prepare *last_promise,
                                                                       const struct paxos_prepare *last_stable_promise) {
    if(write_ahead_window_acceptor_does_instance_ballot_window_need_adjusting(last_stable_promise->ballot, a->min_ballot_catachup, last_promise->ballot)){
        a->last_instance_needs_new_ballot_epoch = true;
        a->last_instance = req->iid;
    }
}

int
write_ahead_window_acceptor_receive_accept(struct write_ahead_window_acceptor* acceptor,
                                    paxos_accept* request, paxos_message* out)
{
    if (request->iid <= acceptor->standard_acceptor->trim_iid) {
        return 0;
    }


    // Check if it is safe to give acceptance (no higher ordered promise or acceptance)
    paxos_prepare* last_promise = calloc(1, sizeof(struct paxos_prepare));
    int found = get_last_promise(acceptor->volatile_storage, request->iid, last_promise);

    if (!found || last_promise->ballot <= request->ballot) {

        if (storage_tx_begin(&acceptor->standard_acceptor->stable_storage) != 0)
            return 0;

        paxos_log_debug("Accepting iid: %u, ballot: %u", request->iid, request->ballot);
        paxos_accept_to_accepted(acceptor->standard_acceptor->id, request, out);
        write_ahead_window_acceptor_store_in_stable_storage(acceptor, &out->u.accepted);
        /*if () != 0) {
            storage_tx_abort(&acceptor->standard_acceptor->stable_storage);
            return 0; //error handling
        }*/


        if (storage_tx_commit(&acceptor->standard_acceptor->stable_storage) != 0)
            return 0;
    } else {
        paxos_accept_to_preempted(acceptor->standard_acceptor->id, request, out);
    }
    free(last_promise);
    return 1;


}

int
write_ahead_window_acceptor_receive_repeat(struct write_ahead_window_acceptor* a, iid_t iid, paxos_accepted* out)
{
    memset(out, 0, sizeof(paxos_accepted));
    if (storage_tx_begin(&a->standard_acceptor->stable_storage) != 0)
        return 0;
    int found = storage_get_instance_info(&a->standard_acceptor->stable_storage, iid, out);
    if (storage_tx_commit(&a->standard_acceptor->stable_storage) != 0)
        return 0;
    return found && (out->value.paxos_value_len > 0);
}

int
write_ahead_window_acceptor_receive_trim(struct write_ahead_window_acceptor* a, paxos_trim* trim)
{
    if (trim->iid <= a->standard_acceptor->trim_iid)
        return 0;
    a->standard_acceptor->trim_iid = trim->iid;
    if (storage_tx_begin(&a->standard_acceptor->stable_storage) != 0)
        return 0;
    storage_store_trim_instance(&a->standard_acceptor->stable_storage, trim->iid);
        if (storage_tx_commit(&a->standard_acceptor->stable_storage) != 0)
        return 0;
    return 1;
}

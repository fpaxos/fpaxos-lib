//
// Created by Michael Davis on 13/11/2019.
//
#include "khash.h"

#include <paxos_message_conversion.h>
#include <epoch_paxos_storage.h>
#include "paxos_storage.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>





#define store_to_hash_map(symbol, map_ptr, map_store_type, key, original_value_to_store, type_freeing_function, copy_method, error) \
    int returned_value; \
    khiter_t iter;   \
    map_store_type* copy_of_value = calloc(1, sizeof(map_store_type)); \
    \
    copy_method(copy_of_value, original_value_to_store); \
    iter = kh_put(symbol, map_ptr, key, &returned_value); \
                \
    if (returned_value == -1) { \
        type_freeing_function(copy_of_value); \
        error = -1; \
    } else if (returned_value == 0) {  \
        type_freeing_function(kh_value(map_ptr, key)); \
    }   \
    kh_value(map_ptr, key) = copy_of_value; \
    error = 0; \




KHASH_MAP_INIT_INT(last_prepares,  struct paxos_prepare*)
KHASH_MAP_INIT_INT(last_accepts,  struct paxos_accept*)
KHASH_MAP_INIT_INT(accepts_epochs, uint32_t*)
KHASH_MAP_INIT_INT(chosen, struct chosen_store*)


struct chosen_store {
    iid_t  instance;
    bool is_chosen;
} ;


struct hash_mapped_memory {
    kh_last_prepares_t* last_prepares;
    kh_last_accepts_t *last_accepts;

    kh_chosen_t *chosen;

    iid_t max_inited_instance;

    int trim_instance_id;
    int aid;
};

struct epoch_hash_mapped_memory {
    kh_accepts_epochs_t *accepts_epochs;
};



// returning 0 means nothing found
// returning 1 means something found§
static int
hash_mapped_memory_get_last_promise(struct hash_mapped_memory *volatile_storage, iid_t instance_id,
                                    paxos_prepare *last_promise_retrieved) {
    khiter_t key = kh_get_last_prepares(volatile_storage->last_prepares, instance_id);//kh_get(PREPARE_MAP_SYMBOL_AND_NAME, volatile_storage->last_prepares, instance_id);
    if (key == kh_end(volatile_storage->last_prepares)) {
        last_promise_retrieved->iid = instance_id;
        last_promise_retrieved->ballot = (struct ballot) {0, 0};
        return 0;
    } else {

        // todo see if there is an issue here
        struct paxos_prepare* prepare =  kh_value(volatile_storage->last_prepares, key);
        paxos_prepare_copy(last_promise_retrieved, prepare);
        return 1;
    }
}


// TODO work out how to add addess this, either different name, then point write bit at end, or some other thing I saw in the internet
static int
hash_mapped_memory_store_last_promise(struct hash_mapped_memory *volatile_storage,
                                      struct paxos_prepare *last_ballot_promised) {
    int error = 0;
   store_to_hash_map(last_prepares, volatile_storage->last_prepares, struct paxos_prepare, last_ballot_promised->iid, last_ballot_promised, paxos_prepare_free, paxos_prepare_copy, error);
    if (last_ballot_promised->iid > volatile_storage->max_inited_instance)
        volatile_storage->max_inited_instance = last_ballot_promised->iid;

    struct paxos_prepare test_prepare;
    memset(& test_prepare, 0, sizeof(test_prepare));
    hash_mapped_memory_get_last_promise(volatile_storage, last_ballot_promised->iid, &test_prepare);
    assert(test_prepare.iid == last_ballot_promised->iid);
    assert(ballot_equal(&test_prepare.ballot, last_ballot_promised->ballot));
    return error;
}

static int
hash_mapped_memory_store_last_prepares(struct hash_mapped_memory *hash_mapped_memory,
                                       paxos_prepare **last_ballots_promised, int number_of_instances) {
    for (int i = 0; i < number_of_instances; i++) {
        hash_mapped_memory_store_last_promise(hash_mapped_memory, last_ballots_promised[i]);
    }
    return 0;
}

static int
hash_mapped_memory_store_last_accepted(struct hash_mapped_memory *volatile_storage,
                                       struct paxos_accept *last_ballot_accepted) {
    int error = 0;
    store_to_hash_map(last_accepts, volatile_storage->last_accepts, struct paxos_accept, last_ballot_accepted->iid, last_ballot_accepted, paxos_accept_free, paxos_accept_copy, error);
    if (last_ballot_accepted->iid > volatile_storage->max_inited_instance)
        volatile_storage->max_inited_instance = last_ballot_accepted->iid;
    return error;
}

static int
hash_mapped_memory_store_last_accepteds(struct hash_mapped_memory *hash_mapped_memory,
                                        paxos_accept **last_ballots_acceptedd, int number_of_instances) {
    for (int i = 0; i < number_of_instances; i++) {
        hash_mapped_memory_store_last_accepted(hash_mapped_memory, last_ballots_acceptedd[i]);
    }
    return 0;
}

static int
hash_mapped_memory_get_last_prepares(struct hash_mapped_memory *volatile_storage, iid_t *instances,
                                     int number_of_instances_to_retrieve, paxos_prepare **last_prepares_retrieved) {
    for (int i = 0; i < number_of_instances_to_retrieve; i++) {
        hash_mapped_memory_get_last_promise(volatile_storage, instances[i], last_prepares_retrieved[i]);
    }
    return 0;
}


static int
hash_mapped_memory_get_last_accepted(struct hash_mapped_memory *volatile_storage, iid_t instance_id,
                                     struct paxos_accept *last_accepted_retrieved) {

    khiter_t key = kh_get_last_accepts(volatile_storage->last_accepts, instance_id);
    if (key == kh_end(volatile_storage->last_accepts)) {
        // not found
        last_accepted_retrieved->iid = instance_id;
        last_accepted_retrieved->ballot = (struct ballot) {.number = 0, .proposer_id = 0};
        last_accepted_retrieved->value = (struct paxos_value) {0, NULL};
        return 0;
    } else {
        // found
        paxos_accept_copy(last_accepted_retrieved, kh_value(volatile_storage->last_accepts, key));
        return 1;
    }
}


static int
hash_mapped_memory_get_last_accepteds(struct hash_mapped_memory *volatile_storage, iid_t *instances,
                                      int number_of_instances_to_retrieve, struct paxos_accept **last_accepteds_retrieved) {
    for (int i = 0; i < number_of_instances_to_retrieve; i++) {
        hash_mapped_memory_get_last_accepted(volatile_storage, instances[i], last_accepteds_retrieved[i]);
    }
    return 0;
}



static void hash_mapped_memory_store_instance_info(struct hash_mapped_memory* paxos_storage,
                                                   const struct paxos_accepted* instance_info) {
    struct paxos_prepare promise_duplicate ;//= calloc(1, sizeof(struct paxos_prepare));
    struct paxos_accept acceptance_duplicate ;//= calloc(1, sizeof(struct paxos_accept));


    paxos_accepted_to_prepare(instance_info, &promise_duplicate);
    paxos_accepted_to_accept(instance_info, &acceptance_duplicate);

    // store also to volatile duplicate
    hash_mapped_memory_store_last_promise(paxos_storage, &promise_duplicate);
    hash_mapped_memory_store_last_accepted(paxos_storage, &acceptance_duplicate);
}

static void hash_mapped_memory_get_instance_info(struct hash_mapped_memory* memory, iid_t instance, struct paxos_accepted* instance_info){
    struct paxos_prepare promise;
    struct paxos_accept accept;

    hash_mapped_memory_get_last_promise(memory, instance, &promise);
    hash_mapped_memory_get_last_accepted(memory, instance, &accept);
    paxos_accepted_from_paxos_prepare_and_accept(&promise, &accept, memory->aid, instance_info);
}


static int
hash_mapped_memory_store_trim_instance(struct hash_mapped_memory *volatile_storage, iid_t trim_instance_id) {
    volatile_storage->trim_instance_id = trim_instance_id;
    return 0;
}

static int
hash_mapped_memory_get_trim_instance(struct hash_mapped_memory *volatile_storage, iid_t *trim_instance_id_retrieved) {
    *trim_instance_id_retrieved = volatile_storage->trim_instance_id;
    return 0;
}


static struct hash_mapped_memory *promises_and_accepts_init_hash_tables() {
    struct hash_mapped_memory* hash_mapped_mem = calloc(1, sizeof(struct hash_mapped_memory));
    hash_mapped_mem->last_prepares = kh_init_last_prepares();//kh_init(PREPARE_MAP_SYMBOL_AND_NAME);
    hash_mapped_mem->last_accepts = kh_init_last_accepts();//kh_init(ACCEPT_MAP_SYMBOL_AND_NAME);
    hash_mapped_mem->chosen = kh_init_chosen();
    hash_mapped_mem->trim_instance_id = MIN_INSTANCE_ID;
    return hash_mapped_mem;
}



static struct hash_mapped_memory *
new_hash_mapped_memory(int aid) {
    struct hash_mapped_memory *hash_mapped_mem = promises_and_accepts_init_hash_tables();
    hash_mapped_mem->aid = aid;
    hash_mapped_mem->trim_instance_id = MIN_INSTANCE_ID;

    return hash_mapped_mem;
}

/*
static struct hash_mapped_memory *
new_hash_mapped_memory_from_promises_and_acceptances(int number_of_initiated_instances_promises, paxos_prepare *promised_instances,
                                                     int number_of_initiated_instances_accepted, paxos_accept *accepted_instances,
                                                     int trim_instance_id) {
    struct hash_mapped_memory *hash_mapped_mem = promises_and_accepts_init_hash_tables();


    hash_mapped_memory_store_last_prepares(hash_mapped_mem, &promised_instances,
                                           number_of_initiated_instances_promises);
    hash_mapped_memory_store_last_accepteds(hash_mapped_mem, &accepted_instances,
                                            number_of_initiated_instances_accepted);
    hash_mapped_mem->trim_instance_id = trim_instance_id;
    return hash_mapped_mem;
}
*/

static struct hash_mapped_memory*
        new_hash_mapped_memory_from_instances_info(struct paxos_accepted* instances_info, int number_of_instances, int trim_instance, int aid){
    struct hash_mapped_memory *hash_mapped_mem = promises_and_accepts_init_hash_tables();
    hash_mapped_mem->trim_instance_id = trim_instance;
    hash_mapped_mem->aid = aid;
    for (int i = 0; i < number_of_instances; i++)
       hash_mapped_memory_store_instance_info(hash_mapped_mem, &instances_info[i]);
    return hash_mapped_mem;
}

// always returns 0 because no errors should appear
static int
        hash_mapped_memory_get_last_inited_instance(const struct hash_mapped_memory* memory, iid_t* max_inited_instance){
    *max_inited_instance = memory->max_inited_instance;
    return 0;
}


static void bool_copy(bool* dst, bool* src){
    memccpy(dst, src, 1, sizeof(bool));
}

static void int_copy(int* dst, int* src) {
    *dst = *src;
}


static int
hash_mapped_memory_is_instance_chosen(const struct hash_mapped_memory* memory, iid_t instance, bool* chosen){
    khiter_t key = kh_get_chosen(memory->chosen, instance);
    if (key == kh_end(memory->chosen)) {
        // not found
        *chosen = false;
        return 0;
    } else {
        // found
        if (kh_value(memory->chosen, key)->is_chosen == true){
            *chosen = true;
        } else {
            *chosen = false;
        }
        return 1;
    }
}

static void chosen_store_copy(struct chosen_store* dst, struct chosen_store* src) {
    dst->is_chosen = src->is_chosen;
    dst->instance = src->instance;
}

static int
hash_mapped_memory_instance_chosen(const struct hash_mapped_memory* memory, iid_t instance){
    int error = 0;

    int rv;
    khiter_t k;
    struct chosen_store* record = malloc(sizeof(struct chosen_store));
    struct chosen_store value = {instance, 1};
    chosen_store_copy(record, &value);
    k = kh_put_chosen(memory->chosen, instance, &rv);
    if (rv == -1) { // error
        free(record);
        return -1;
    }
    if (rv == 0) { // key is already present
        free(kh_value(memory->chosen, k));
    }
    kh_value(memory->chosen, k) = record;
  //  int* tmp = calloc(1, sizeof(int));
  //  *tmp = 1;
  //  store_to_hash_map(chosen, memory->chosen, struct chosen_store, instance, (struct chosen_store*){1}, free, chosen_store_copy, error);
 //   free(tmp);
    return error;
}

static void
initialise_hash_mapped_memory_function_pointers(struct paxos_storage *volatile_storage) {
    volatile_storage->api.get_max_inited_instance = (int (*) (void *, iid_t*)) hash_mapped_memory_get_last_inited_instance;
    volatile_storage->api.get_trim_instance = (int (*) (void *, iid_t *)) hash_mapped_memory_get_trim_instance;
    volatile_storage->api.store_trim_instance = (int (*) (void *, iid_t)) hash_mapped_memory_store_trim_instance;

    volatile_storage->api.get_last_accepted = (int (*) (void *, iid_t, struct paxos_accept *)) hash_mapped_memory_get_last_accepted;
    volatile_storage->api.get_last_accepteds = (int (*) (void *, iid_t *, int, struct paxos_accept **)) hash_mapped_memory_get_last_accepteds; // TODO add number of accepteds
    volatile_storage->api.store_last_accepted = (int (*) (void *, struct paxos_accept* )) hash_mapped_memory_store_last_accepted;
    volatile_storage->api.store_last_accepteds = (int (*) (void *, struct paxos_accept **, int)) hash_mapped_memory_store_last_accepteds;

    volatile_storage->api.get_last_promise = (int (*) (void *, iid_t, struct paxos_prepare *)) hash_mapped_memory_get_last_promise;
    volatile_storage->api.get_last_promises = (int (*) (void *, iid_t *, int, struct paxos_prepare **, int *)) hash_mapped_memory_get_last_prepares;
    volatile_storage->api.store_last_promise = (int (*) (void *, const struct paxos_prepare *)) hash_mapped_memory_store_last_promise;
    volatile_storage->api.store_last_promises = (int (*) (void *, struct paxos_prepare **, int)) hash_mapped_memory_store_last_prepares;

    volatile_storage->api.set_instance_chosen = (int (*) (void *, iid_t)) hash_mapped_memory_instance_chosen;
    volatile_storage->api.is_instance_chosen = (int (*) (void *, iid_t, bool*)) hash_mapped_memory_is_instance_chosen;

    volatile_storage->api.get_instance_info = (int (*) (void*, iid_t, struct paxos_accepted*)) hash_mapped_memory_get_instance_info;

    volatile_storage->api.store_instance_info = (int (*) (void *, const struct paxos_accepted *)) hash_mapped_memory_store_instance_info;
    // TODO get all untrimmed instances -- not important

}


void init_hash_mapped_memory(struct paxos_storage* paxos_storage, int aid){
    initialise_hash_mapped_memory_function_pointers(paxos_storage);
    paxos_storage->handle = new_hash_mapped_memory(aid);
}

void init_hash_mapped_memory_from_instances_info(struct paxos_storage* paxos_storage, struct paxos_accepted* instances_info, int number_of_instances, int trim_instance, int aid){
    initialise_hash_mapped_memory_function_pointers(paxos_storage);
    paxos_storage->handle = new_hash_mapped_memory_from_instances_info(instances_info, number_of_instances, trim_instance, aid);
}


// TODO add epohc_hash_mapped_memory init, get epoch, store epoch
static int  epoch_hash_mapped_memory_get_accept_epoch(struct epoch_hash_mapped_memory* epoch_hash_mapped_memory, iid_t instance, uint32_t* retrieved_epoch){
    khiter_t key = kh_get_accepts_epochs(epoch_hash_mapped_memory->accepts_epochs, instance);
    if (key == kh_end(epoch_hash_mapped_memory->accepts_epochs)) {
        // not found
        *retrieved_epoch = 0;
        return 0;
    } else {
        // found
        *retrieved_epoch = *kh_value(epoch_hash_mapped_memory->accepts_epochs, key);
        return 1;
    }
}

static void uint32_copy(uint32_t* dst, const uint32_t* src){
    *dst = *src;
}

static int epoch_hash_mapped_memory_store_accept_epoch(struct epoch_hash_mapped_memory* epoch_hash_mapped_memory, const iid_t instance, const uint32_t epoch_to_store) {
    int error = 0;
    store_to_hash_map(accepts_epochs,     epoch_hash_mapped_memory->accepts_epochs, uint32_t, instance,&epoch_to_store, free, uint32_copy, error)
    return error;
}

struct epoch_hash_mapped_memory* new_epoch_hash_mapped_memory() {
    struct epoch_hash_mapped_memory* memory = calloc(1, sizeof(struct epoch_hash_mapped_memory));
    memory->accepts_epochs = kh_init_accepts_epochs();//kh_init(EPOCH_MAP_SYMBOL_AND_NAME);
    return memory;
}

void epoch_hash_mapped_memory_init(struct epoch_paxos_storage* memory, int aid){
    init_hash_mapped_memory(&memory->paxos_storage, aid);

    memory->extended_handle = new_epoch_hash_mapped_memory();

    memory->extended_api.get_accept_epoch = (int (*) (void*, iid_t, uint32_t *)) epoch_hash_mapped_memory_get_accept_epoch;
    memory->extended_api.store_accept_epoch = (int (*) (void*, iid_t, uint32_t)) epoch_hash_mapped_memory_store_accept_epoch; // set pointers
}


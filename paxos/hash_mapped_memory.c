//
// Created by Michael Davis on 13/11/2019.
//
#include <paxos_storage.h>
#include <paxos_message_conversion.h>
#include "paxos_storage.h"
#include "khash.h"


KHASH_MAP_INIT_INT(last_promises, paxos_prepare*);
KHASH_MAP_INIT_INT(last_accepteds, paxos_accept*);

struct hash_mapped_memory {
    kh_last_promises_t *last_promises;
    kh_last_accepteds_t *last_accepteds;

    int trim_instance_id;
};



// returning 0 means nothing found
// returning 1 means something found§
static int
hash_mapped_memory_get_last_promise(struct hash_mapped_memory *volatile_storage, iid_t instance_id,
                                    paxos_prepare *last_promise_retrieved) {
    khiter_t key = kh_get_last_promises(volatile_storage->last_promises, instance_id);
    if (key == kh_end(volatile_storage->last_promises)) {
        last_promise_retrieved->iid = instance_id;
        return 0;
    } else {
        paxos_prepare_copy(last_promise_retrieved, kh_value(volatile_storage->last_promises, key));
        return 1;
    }
}


// TODO work out how to add addess this, either different name, then point write bit at end, or some other thing I saw in the internet
static int
hash_mapped_memory_store_last_promise(struct hash_mapped_memory *volatile_storage,
                                      struct paxos_prepare *last_ballot_promised) {
    // Variables
    int returned_value;
    khiter_t key;
    struct paxos_prepare* to_store_prepare = calloc(1, sizeof(struct paxos_prepare));

    // Copy to new memory so kh can point towards it
    paxos_prepare_copy(to_store_prepare, last_ballot_promised);
    key = kh_put_last_promises(volatile_storage->last_promises, last_ballot_promised->iid, &returned_value);

    if (returned_value == -1) { //error has occured
        return -1;
    } else if (returned_value == 0) { // key is already present
        paxos_prepare_free(kh_value(volatile_storage->last_promises, key));
    }
    kh_value(volatile_storage->last_promises, key) = to_store_prepare; // poinot kh_value to copied prepare
    return 0;
}

static int
hash_mapped_memory_store_last_promises(struct hash_mapped_memory *hash_mapped_memory,
                                       paxos_prepare **last_ballots_promised, int number_of_instances) {
    for (int i = 0; i < number_of_instances; i++) {
        hash_mapped_memory_store_last_promise(hash_mapped_memory, last_ballots_promised[i]);
    }
    return 0;
}

static int
hash_mapped_memory_store_last_accepted(struct hash_mapped_memory *volatile_storage,
                                       paxos_accept *last_ballot_accepted) {
    // Variables
    int returned_value;
    khiter_t key;
    struct paxos_accept* to_store_accept = calloc(1, sizeof(struct paxos_accept));

    // Copy to new memory so kh can point towards it
    paxos_accept_copy(to_store_accept, last_ballot_accepted);
    key = kh_put_last_accepteds(volatile_storage->last_accepteds, last_ballot_accepted->iid, &returned_value);

    if (returned_value == -1) { //error has occured
        return -1;
    } else if (returned_value == 0) { // key is already present
        paxos_accept_free(kh_value(volatile_storage->last_accepteds, key));
    }
    kh_value(volatile_storage->last_accepteds, key) = to_store_accept; // poinot kh_value to copied prepare
    return 0;
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
hash_mapped_memory_get_last_promises(struct hash_mapped_memory *volatile_storage, iid_t *instances,
                                     int number_of_instances_to_retrieve, paxos_prepare **last_promises_retrieved) {
    for (int i = 0; i < number_of_instances_to_retrieve; i++) {
        hash_mapped_memory_get_last_promise(volatile_storage, instances[i], last_promises_retrieved[i]);
    }
    return 0;
}


static int
hash_mapped_memory_get_last_accepted(struct hash_mapped_memory *volatile_storage, iid_t instance_id,
                                     struct paxos_accept *last_accepted_retrieved) {

    khiter_t key = kh_get_last_accepteds(volatile_storage->last_accepteds, instance_id);
    if (key == kh_end(volatile_storage->last_accepteds)) {
        // not found
        last_accepted_retrieved->iid = instance_id;
        last_accepted_retrieved->ballot = 0;
        last_accepted_retrieved->value = (struct paxos_value) {0, NULL};
        return 0;
    } else {
        // found
        paxos_accept_copy(last_accepted_retrieved, kh_value(volatile_storage->last_accepteds, key));
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


static struct hash_mapped_memory *allocate_memory_and_init_hash_tables() {
    struct hash_mapped_memory* hash_mapped_mem = calloc(1, sizeof(struct hash_mapped_memory));
    hash_mapped_mem->last_promises = kh_init_last_promises();
    hash_mapped_mem->last_accepteds = kh_init_last_accepteds();
    return hash_mapped_mem;
}



static struct hash_mapped_memory *
new_hash_mapped_memory() {
    struct hash_mapped_memory *hash_mapped_mem = allocate_memory_and_init_hash_tables();
    hash_mapped_mem->trim_instance_id = MIN_INSTANCE_ID;
    return hash_mapped_mem;
}

static struct hash_mapped_memory *
new_hash_mapped_memory_from_promises_and_acceptances(int number_of_initiated_instances_promises, paxos_prepare *promised_instances,
                                                     int number_of_initiated_instances_accepted, paxos_accept *accepted_instances,
                                                     int trim_instance_id) {
    struct hash_mapped_memory *hash_mapped_mem = allocate_memory_and_init_hash_tables();


    hash_mapped_memory_store_last_promises(hash_mapped_mem, &promised_instances,
                                           number_of_initiated_instances_promises);
    hash_mapped_memory_store_last_accepteds(hash_mapped_mem, &accepted_instances,
                                            number_of_initiated_instances_accepted);
    hash_mapped_mem->trim_instance_id = trim_instance_id;
    return hash_mapped_mem;
}


static struct hash_mapped_memory*
        new_hash_mapped_memory_from_instances_info(struct paxos_accepted* instances_info, int number_of_instances, int trim_instance){
    struct hash_mapped_memory *hash_mapped_mem = allocate_memory_and_init_hash_tables();
    hash_mapped_mem->trim_instance_id = trim_instance;
    for (int i = 0; i < number_of_instances; i++)
       hash_mapped_memory_store_instance_info(hash_mapped_mem, &instances_info[i]);
    return hash_mapped_mem;
}



static void
initialise_hash_mapped_memory_function_pointers(struct paxos_storage *volatile_storage) {
    volatile_storage->api.get_trim_instance = (int (*) (void *, iid_t *)) hash_mapped_memory_get_trim_instance;
    volatile_storage->api.store_trim_instance = (int (*) (void *, iid_t)) hash_mapped_memory_store_trim_instance;

    volatile_storage->api.get_last_accepted = (int (*) (void *, iid_t, struct paxos_accept *)) hash_mapped_memory_get_last_accepted;
    volatile_storage->api.get_last_accepteds = (int (*) (void *, iid_t *, int, struct paxos_accept **)) hash_mapped_memory_get_last_accepteds; // TODO add number of accepteds
    volatile_storage->api.store_last_accepted = (int (*) (void *, struct paxos_accept* )) hash_mapped_memory_store_last_accepted;
    volatile_storage->api.store_last_accepteds = (int (*) (void *, struct paxos_accept **, int)) hash_mapped_memory_store_last_accepteds;

    volatile_storage->api.get_last_promise = (int (*) (void *, iid_t, struct paxos_prepare *)) hash_mapped_memory_get_last_promise;
    volatile_storage->api.get_last_promises = (int (*) (void *, iid_t *, int, struct paxos_prepare **, int *)) hash_mapped_memory_get_last_promises;
    volatile_storage->api.store_last_promise = (int (*) (void *, const struct paxos_prepare *)) hash_mapped_memory_store_last_promise;
    volatile_storage->api.store_last_promises = (int (*) (void *, struct paxos_prepare **, int)) hash_mapped_memory_store_last_promises;

    //volatile_storage->api.get_instance_info = hash_mapped_memory_get_instance_info;

    volatile_storage->api.store_instance_info = (int (*) (void *, const struct paxos_accepted *)) hash_mapped_memory_store_instance_info;
    // TODO get all untrimmed instances

}


void init_hash_mapped_memory(struct paxos_storage* paxos_storage){
    initialise_hash_mapped_memory_function_pointers(paxos_storage);
    paxos_storage->handle = new_hash_mapped_memory();
}

void init_hash_mapped_memory_from_instances_info(struct paxos_storage* paxos_storage, struct paxos_accepted* instances_info, int number_of_instances, int trim_instance){
    initialise_hash_mapped_memory_function_pointers(paxos_storage);
    paxos_storage->handle = new_hash_mapped_memory_from_instances_info(instances_info, number_of_instances, trim_instance);
}
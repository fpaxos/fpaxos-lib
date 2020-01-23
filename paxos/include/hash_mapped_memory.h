//
// Created by Michael Davis on 14/11/2019.
//

#include "paxos_types.h"

#ifndef LIBPAXOS_HASH_MAPPED_MEMORY_H
#define LIBPAXOS_HASH_MAPPED_MEMORY_H


//void initialise_hash_mapped_memory_function_pointers(struct paxos_storage *volatile_storage);

//struct hash_mapped_memory *new_hash_mapped_memory();
/*
struct hash_mapped_memory *
new_hash_mapped_memory_from_promises_and_acceptances(int number_of_initiated_instances_promises, paxos_prepare *promised_instances,
                                                     int number_of_initiated_instances_accepted, paxos_accept *accepted_instances,
                                                     int trim_instance_id);

struct hash_mapped_memory*
new_hash_mapped_memory_from_instances_info(paxos_accepted** instances_info, int* number_of_promises);
 */

void init_hash_mapped_memory(struct paxos_storage* paxos_storage);

void init_hash_mapped_memory_from_instances_info(struct paxos_storage* paxos_storage, struct paxos_accepted* instances_info, int number_of_instances, int trim_instance);

#endif //LIBPAXOS_HASH_MAPPED_MEMORY_H

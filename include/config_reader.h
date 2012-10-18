#ifndef _CONFIG_READER_H_
#define _CONFIG_READER_H_

#include "paxos_net.h"

#define MAX_ADDR 10

typedef struct conf_t {
	int learners_count;
	int proposers_count;
	int acceptors_count;
	address learners[MAX_ADDR];
	address proposers[MAX_ADDR];
	address acceptors[MAX_ADDR];
} config;

config* read_config(const char* path);

#endif

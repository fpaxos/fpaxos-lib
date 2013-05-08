#ifndef _CONFIG_READER_H_
#define _CONFIG_READER_H_

#define MAX_ADDR 10

typedef struct address_t {
	char* address_string;
	int port;
} address;

struct config
{
	int proposers_count;
	int acceptors_count;
	address proposers[MAX_ADDR];
	address acceptors[MAX_ADDR];
};

struct config* read_config(const char* path);

#endif

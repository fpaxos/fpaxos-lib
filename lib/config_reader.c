#include "config_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int fields = 4;

static void print_config(address* a, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		printf("%s %d\n", a[i].address_string, a[i].port);
	}
}

struct config* read_config(const char* path)
{
	int id;
	char type;
	address a;
	address* tmp;
	struct config* c;
	FILE* f;

	f = fopen(path, "r");
	if (f == NULL) {
		perror("fopen"); return NULL;
	}
	
	c = malloc(sizeof(struct config));
	memset(c, 0, sizeof(struct config));
	a.address_string = malloc(128);
	
	while(fscanf(f, "%c %d %s %d\n", &type, &id,
		a.address_string, &a.port) == fields) {
			
		switch(type) {
			case 'l':
				tmp = &c->learners[c->learners_count++];
				tmp->port = a.port;
				tmp->address_string = malloc(128);
				memcpy(tmp->address_string, a.address_string, 128);
				break;
			case 'p':
				tmp = &c->proposers[c->proposers_count++];
				tmp->port = a.port;
				tmp->address_string = malloc(128);
				memcpy(tmp->address_string, a.address_string, 128);
				break;
			case 'a':
				tmp = &c->acceptors[c->acceptors_count++];
				tmp->port = a.port;
				tmp->address_string = malloc(128);
				memcpy(tmp->address_string, a.address_string, 128);
				break;
		}
	}
	
	printf("learners\n");
	print_config(c->learners, c->learners_count);
	printf("proposers\n");
	print_config(c->proposers, c->proposers_count);
	printf("acceptors\n");
	print_config(c->acceptors , c->acceptors_count);

	return c;
}

#include "config_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int fields = 4;

static void print_config(address* a, int count) {
	int i;
	for (i = 0; i < count; i++) {
		printf("%s %d\n", a[i].address_string, a[i].port);
	}
}

config* read_config(const char* path) {
	int id;
	char type;
	address a;
	config* c;
	FILE* f;

	f = fopen(path, "r");
	if (f == NULL) {
		perror("fopen"); return NULL;
	}
	
	c = malloc(sizeof(config));
	a.address_string = malloc(128);
	
	memset(c, 0, sizeof(config));
	while(fscanf(f, "%c %d %s %d\n", &type, &id,
		a.address_string, &a.port) == fields) {
		switch(type) {
			case 'l':
			c->learners[c->learners_count++] = a;
			break;
			case 'p':
			c->proposers[c->proposers_count++] = a;
			break;
			case 'a':
			c->acceptors[c->acceptors_count++] = a;
			break;
			default:
			printf("Error in config file\n");
			return NULL;
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

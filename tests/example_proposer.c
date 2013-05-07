#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "evpaxos.h"

void
handle_cltr_c(int sig)
{
	printf("Caught signal %d\n", sig);
	exit(0);
}

int
main (int argc, char const *argv[])
{
	int id;
	struct event_base* base;
		
	signal(SIGINT, handle_cltr_c);

	if (argc != 3) {
		printf("Usage: %s id config\n", argv[0]);
		exit(1);
	}

	base = event_base_new();    
	id = atoi(argv[1]);
	
	if (proposer_init(id, argv[2], base) == NULL) {
		printf("Could not start the proposer!\n");
		exit(1);
	}

	event_base_dispatch(base);
	return 0;
}

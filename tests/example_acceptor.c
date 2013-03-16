#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "libpaxos.h"

static int acceptor_id;
static struct acceptor* a;
static struct event_base* b;

void
handle_cltr_c (int sig) {
	printf("Caught signal %d\n", sig);
	acceptor_exit(a);
	exit(0);
}

int 
main (int argc, char const *argv[])
{	
	signal(SIGINT, handle_cltr_c);
    
	if (argc != 3) {
		printf("Usage: %s id config\n", argv[0]);
		exit(1);
	}
	
	acceptor_id = atoi(argv[1]);
	
	b = event_base_new();
	a = acceptor_init(acceptor_id, argv[2], b);
	
	if (a == NULL) {
		printf("Could not start the acceptor\n");
		return 0;
	}
	
	event_base_dispatch(b);

	return 1;
}

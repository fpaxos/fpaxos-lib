#include <stdlib.h>
#include <stdio.h>
#include <evpaxos.h>
#include <signal.h>

void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

int
main (int argc, char const *argv[])
{
	int id;
	struct event* sig;
	struct event_base* base;

	if (argc != 3) {
		printf("Usage: %s id config\n", argv[0]);
		exit(1);
	}

	base = event_base_new();    

	id = atoi(argv[1]);
	if (evproposer_init(id, argv[2], base) == NULL) {
		printf("Could not start the proposer!\n");
		exit(1);
	}
	
	sig = evsignal_new(base, SIGINT, handle_sigint, base);
	evsignal_add(sig, NULL);	
	
	event_base_dispatch(base);
	return 0;
}

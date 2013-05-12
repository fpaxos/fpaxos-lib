#include <stdlib.h>
#include <stdio.h>
#include <evpaxos.h>

void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

static void
deliver(char* value, size_t size, iid_t iid, ballot_t b, 
	int proposer, void* arg)
{
	printf("Paxos instance %u closed by ballot %u\n", iid, b);
	printf("%s\n", value);
}

int
main(int argc, char const *argv[])
{
	struct event* sig;
	struct evlearner* lea;
	struct event_base* base;

	if (argc != 2) {
		printf("Usage: %s config\n", argv[0]);
		exit(1);
	}

	base = event_base_new();

	lea = evlearner_init(argv[1], deliver, NULL, base);
	if (lea == NULL) {
		printf("Could not start the learner!\n");
		exit(1);
	}
	
	sig = evsignal_new(base, SIGINT, handle_sigint, base);
	evsignal_add(sig, NULL);
	
	event_base_dispatch(base);
	return 0;
}

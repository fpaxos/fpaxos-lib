#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "libpaxos.h"

void
handle_cltr_c (int sig)
{
	printf("Caught signal %d\n", sig);
    exit(0);
}

static char
as_char(char c)
{
    if(c < 33 || c > 126) {
        return '!';        
    } else {
        return c;
    }
}

static void
deliver(char* value, size_t size, iid_t iid, ballot_t b, 
	int proposer, void* arg)
{
    printf("Paxos instance %u closed by ballot %u\n", iid, b);
    printf("Value (by proposer:%d, size: %d) ->", proposer, (int)size);
    printf("[%c][%c][%c][...]\n", as_char(value[0]), as_char(value[1]), as_char(value[2]));
}

int
main (int argc, char const *argv[])
{
	struct learner* l;
	struct event_base* b;

    signal(SIGINT, handle_cltr_c);
	
	if (argc != 2) {
        printf("Usage: %s config\n", argv[0]);
        exit(1);
    }

	b = event_base_new();
	l = learner_init(argv[2], deliver, NULL, b);
	
    if (l == NULL) {
        printf("Could not start the learner!\n");
        exit(1);
    }
	event_base_dispatch(b);
    return 0;
}

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>

#include <event2/event.h>
#include <event2/event_struct.h>
#include "libpaxos.h"

#define UPDATE_INTERVAL 10

static struct event sample_ev;
static struct timeval sample_time;
static long unsigned int bytes = 0;
static long unsigned int values = 0;

static void
init_counters()
{
	bytes = 0;
	values = 0;
}

static void
sample(int fd, short ev, void* arg)
{
	double vps, mbps;
	
	vps = (double)values / UPDATE_INTERVAL;
	mbps = (((double)bytes*8) / UPDATE_INTERVAL) / 1000000;
	
	printf("%f %f\n", vps, mbps);
	
	init_counters();
	evtimer_add(&sample_ev, &sample_time);
}

static void 
deliver(char* v, size_t s, iid_t iid, ballot_t b, int proposer, void* arg)
{
	bytes += s;
	values += 1;
}


void 
handle_cltr_c (int sig)
{
	printf("Caught signal %d\n", sig);
    exit(0);
}


int 
main (int argc, char const *argv[])
{
	struct learner* l;
	struct event_base *b;
	
    signal(SIGINT, handle_cltr_c);

	if (argc != 3) {
        printf("Usage: %s id config\n", argv[0]);
        exit(1);
    }
    
	b = event_base_new();
	l = learner_init(argv[2], deliver, NULL, b);
	
    if (l == NULL) {
        printf("Could not start the learner!\n");
		exit(1);
    }

	init_counters();
    sample_time.tv_sec = UPDATE_INTERVAL;
    sample_time.tv_usec = 0;

	evtimer_assign(&sample_ev, b, sample, NULL);
	evtimer_add(&sample_ev, &sample_time);
	
	event_base_dispatch(b);
	
	return 0;
}

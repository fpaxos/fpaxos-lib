#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>

#include "event.h"
#include "evutil.h"

#include "libpaxos.h"

//Interval of throughput update print (in seconds)
#define UPDATE_INTERVAL 1

long unsigned int last_sample_bytes = 0;
long unsigned int last_sample_delivered = 0;
long unsigned int total_bytes = 0;
long unsigned int total_delivered = 0;
struct timeval monitor_start;
struct timeval sample_start;

int monitor_initialized = 0;

struct event update_check_event;
struct timeval update_check_interval;

FILE * logfile = NULL;

//Does t1 = t1 - t2 [t2 happens before: t2 < t1]
void timeval_subtract(struct timeval * t1, struct timeval * t2) {
    // printf("%lu.%u - %lu.%u =", 
    //     (t1->tv_sec % 3600), t1->tv_usec, (t2->tv_sec % 3600), t2->tv_usec);
    t1->tv_sec -= t2->tv_sec;
    if(t2->tv_usec < t1->tv_usec) {
        t1->tv_usec -= t2->tv_usec;
    } else {
        t1->tv_sec -= 1;
        t1->tv_usec += 1000000;
        t1->tv_usec -= t2->tv_usec;
    }
    
    // printf("%lu.%u\n", t1->tv_sec, t1->tv_usec);
}

void periodic_check(int sock, short event, void *arg) {
    //Set next periodic check event
    event_add(&update_check_event, &update_check_interval);
    
    //Still waiting for the first value to be delivered
    if(!monitor_initialized) {
        return;
    }
    
    //Get (2 copies) of current time
    struct timeval current_time, elapsed_time;
    gettimeofday(&current_time, NULL);
    elapsed_time.tv_sec = current_time.tv_sec;
    elapsed_time.tv_usec = current_time.tv_usec;
    
    //How much time from sample start?
    timeval_subtract(&elapsed_time, &sample_start);
    
    //Time to print, update and reset
    if(elapsed_time.tv_sec >= UPDATE_INTERVAL || arg != NULL) {
        double secs, vps, kbps;
        
        //Calculate throughput for last sample
        secs = (((float)elapsed_time.tv_usec)/100000)+elapsed_time.tv_sec;
        vps = ((float)last_sample_delivered/secs); 
        kbps = ((float)last_sample_bytes/secs)/1000;
        
        fprintf(logfile, "%.1f, %lu\n", secs, last_sample_delivered); 
        //Print stats for last sample
        // printf("Last sample: %lu values (%lu bytes) in %.1f secs\n", 
        //     last_sample_delivered, last_sample_bytes, secs);
        // printf("TP: %.2f v/s, %.2f kb/s\n", vps, kbps);
        
        //Update total counter
        total_delivered += last_sample_delivered;
        total_bytes += last_sample_bytes;
        //How much time from delivery of first value?
        timeval_subtract(&current_time, &monitor_start);

        //Calculate throughput overall
        // secs = (((float)current_time.tv_usec)/100000)+current_time.tv_sec;
        // vps = ((float)total_delivered/secs); 
        // kbps = ((float)total_bytes/secs)/1000;        
        // 
        // //Print stats overall
        // printf("Total: %lu values (%lu bytes) in %.1f secs\n",
        //     total_delivered, total_bytes, secs);
        // printf("TP: %.2f v/s, %.2f kb/s\n", vps, kbps);
        
        //Reset last sample stats and timer
        last_sample_delivered = 0;
        last_sample_bytes = 0;
        gettimeofday(&sample_start, NULL);
    }
    
    //Makes the compiler happy
    sock = sock; event = event; arg = arg;
}

void my_deliver_fun(char* value, size_t value_size, iid_t iid, ballot_t ballot, int proposer) {
    
    if(!monitor_initialized) {
        monitor_initialized = 1;
        // assert(iid == 1);
        gettimeofday(&monitor_start, NULL);
        sample_start.tv_sec = monitor_start.tv_sec;
        sample_start.tv_usec = monitor_start.tv_usec;
        printf("Monitoring started\n");
    }
    
    last_sample_bytes += value_size;
    last_sample_delivered += 1;
    assert((total_delivered + last_sample_delivered) == iid);
    
    //Makes the compiler happy
    value = value; iid = iid, ballot = ballot; proposer = proposer;
}

int my_custom_init() {
    
    //Set the periodic check event, every second
    update_check_interval.tv_sec = 1;
    update_check_interval.tv_usec = 0;
    evtimer_set(&update_check_event, periodic_check, NULL);    
    event_add(&update_check_event, &update_check_interval);
    
    return 0;
}

void handle_cltr_c (int sig) {
	printf("Caught signal %d\n", sig);
    periodic_check(0, 0, (void*)0xFF);
    printf("%lu values delivered\n", total_delivered);
    fclose(logfile);
    exit(0);
}


int main (int argc, char const *argv[]) {
    signal(SIGINT, handle_cltr_c);
    
    if (argc != 2) { 
        printf("Must pass the name of log file as argument\n");
        return -1;
    }
    logfile = fopen(argv[1], "w");
    if (logfile == NULL) {
        printf("Invalid file: %s\n", argv[1]);
        perror("fopen:");
        return -1;
    }
    
    
    
    if (learner_init(my_deliver_fun, my_custom_init) != 0) {
        printf("Could not start the learner!\n");
        return -1;
    }
    
    while(1) {
        //This thread does nothing...
        //But it can't terminate!
        sleep(10);
    }
    


    //Makes the compiler happy....
    argc = argc;
    argv = argv;
    return 0;
}
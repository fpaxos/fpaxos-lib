#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#include "event.h"
#include "libpaxos.h"

static int start_time;
static int end_time;
static int force_exit = 0;
 
static int delivered_count = 0;
static int submitted_count = 0;
static int retried_count = 0;

static struct event cl_periodic_event;
static struct timeval cl_periodic_interval;

typedef struct client_value_record_t {
    struct timeval creation_time;
    struct timeval expire_time;
    size_t value_size;
    char value[PAXOS_MAX_VALUE_SIZE];
} client_value_record;

static client_value_record * values_table;
 
static paxos_submit_handle * psh = NULL;

//Parameters
unsigned int concurrent_values = 30;
int min_val_size = 30;
int max_val_size = PAXOS_MAX_VALUE_SIZE;
int duration = 40;
int print_step = 10;
int wait_after_init=0;
struct timeval values_timeout;

//Latency statistics
static struct timeval min_latency;
static struct timeval max_latency;
static struct timeval aggregated_latency;
static long unsigned int aggregated_latency_count = 0;
//Sampling random values
static int sample_frequency = 0;
static int max_samples = 500;
static double * samples;
static int samples_count = 0;

void pusage() {
    printf("benchmark_client options:\n");
    printf("\t-c N : submit N values concurrently\n");
    printf("\t-m N : min value size is N bytes\n");
    printf("\t-M N : max value size is N bytes\n");
    printf("\t-d N : duration is N seconds\n");
    printf("\t-t N : submit timeout is N seconds\n");
    printf("\t-p N : print submit count every N values\n");
    printf("\t-s N : saves a latency sample every N values sent\n");
    printf("\t-w N : after initialization is completed, wait N seconds before submitting\n");
    printf("\t-h   : prints this message\n");    
}

void handle_cltr_c (int sig) {
	printf("Caught signal %d\n", sig);
    force_exit = 1;
}

void parse_args(int argc, char * const argv[]) {

    int c;
    while((c = getopt(argc, argv, "c:m:M:d:t:p:s:w:h")) != -1) {
        switch(c) {
            case 'c': {
                concurrent_values = atoi(optarg);
            }
            break;

            case 'm': {
                min_val_size = atoi(optarg);
            }
            break;

            case 'M': {
                max_val_size = atoi(optarg);
            }
            break;

            case 'd': {
                duration = atoi(optarg);
            }
            break;

            case 't': {
                values_timeout.tv_sec = atoi(optarg);
            }
            break;

            case 'p': {
                print_step = atoi(optarg);
            }
            break;
            
            case 's': {
                sample_frequency = atoi(optarg);
                samples = malloc(sizeof(double)*max_samples);
            }
            break;
            
            case 'w': {
                wait_after_init = atoi(optarg);
            }
            break;

            
            case 'h':
            default: {
                pusage();
                exit(0);
            }
        }
    }    
}

static void
print_benchmark_parameters () {
    printf("sample_frequency set to: %d\n", sample_frequency);
    printf("print_step set to: %d\n", print_step);
    printf("values_timeout set to: %d\n", (int)values_timeout.tv_sec);
    printf("concurrent_values set to: %d\n", concurrent_values);
    printf("min_val_size set to: %d\n", min_val_size);
    printf("max_val_size set to: %d\n", max_val_size);
    printf("duration set to: %d\n", duration);
    printf("Initial submission delay set to: %d\n", wait_after_init);   
}

static void 
sum_timevals(struct timeval * dest, struct timeval * t1, struct timeval * t2) {
    dest->tv_sec = t1->tv_sec + t2->tv_sec;
    unsigned int usecs = t1->tv_usec + t2->tv_usec;
    if(usecs > 1000000) {
        dest->tv_usec = (usecs % 1000000);
        dest->tv_sec += 1;
    } else {
        dest->tv_usec = usecs;
    }
}

//Returns 1 if t1 > t2
static int
is_gt_timeval(struct timeval * t1, struct timeval * t2) {
    return (t1->tv_sec > t2->tv_sec) ||
        (t1->tv_sec == t2->tv_sec && t1->tv_usec > t2->tv_usec);
}

//Returns 1 if t1 < t2
static int
is_lt_timeval(struct timeval * t1, struct timeval * t2) {
    return (t1->tv_sec < t2->tv_sec) ||
        (t1->tv_sec == t2->tv_sec && t1->tv_usec < t2->tv_usec);
}

//Does t1 = t1 + t2
static void
sum_in_place_timevals(struct timeval * t1, struct timeval * t2) {
    t1->tv_sec += t2->tv_sec;
    t1->tv_usec += t2->tv_usec;
    if(t1->tv_usec >= 1000000) {
        t1->tv_sec += 1;
        t1->tv_usec = (t1->tv_usec % 1000000);
    }
}

//Does t1 = t1 - t2 [t2 must be before (<=) t1]
static void
subtract_in_place_timevals(struct timeval * t1, struct timeval * t2) {
    t1->tv_sec -= t2->tv_sec;
    if(t2->tv_usec < t1->tv_usec) {
        t1->tv_usec -= t2->tv_usec;
    } else {
        t1->tv_sec -= 1;
        t1->tv_usec += 1000000;
        t1->tv_usec -= t2->tv_usec;
    }
}


static void 
save_latency_info (client_value_record * cvr, struct timeval * curr_time) {
    //Calculate time from 1st submit to deliver
    subtract_in_place_timevals(curr_time, &cvr->creation_time);
    struct timeval * time_taken = curr_time;
    
    //If greater than max store
    if(is_gt_timeval(time_taken, &max_latency)) {
        max_latency.tv_sec = time_taken->tv_sec;
        max_latency.tv_usec = time_taken->tv_usec;
    }

    //If less than min store
    if(is_lt_timeval(time_taken, &min_latency)) {
        min_latency.tv_sec = time_taken->tv_sec;
        min_latency.tv_usec = time_taken->tv_usec;
    }
    
    sum_in_place_timevals(&aggregated_latency, time_taken);
    aggregated_latency_count += 1;
    
    //Save a latency sample every sample_frequency 
    // values successfully submitted
    if(sample_frequency != 0 && 
    samples_count < max_samples && 
    aggregated_latency_count % sample_frequency == 0) {
        samples[samples_count] = ((float)time_taken->tv_sec*1000) +
            ((float)time_taken->tv_usec/1000);
        samples_count += 1;
    }
}

static void 
submit_old_value(client_value_record * cvr) {
    retried_count += 1;

    //Leave value, value size and creation time unaltered

    //Set expiration as now+timeout
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    sum_timevals(&cvr->expire_time, &time_now, &values_timeout);
    
    //Send the value to proposers and return immediately
    pax_submit_nonblock(psh, cvr->value, cvr->value_size);
}

size_t random_value_gen(char * buf) {
    long r = random();
    int val_size = (r % ((max_val_size - min_val_size)+1)) + min_val_size;
    size_t passes = val_size / sizeof(long);
    long * ary = (long*) buf;
    size_t i;
    for(i = 0; i < passes; i++) {
        ary[i] = r;
    }
    return i*sizeof(long);
}

static void 
submit_new_value(client_value_record * cvr) {
    
    submitted_count += 1;
    if((submitted_count % print_step) == 0) {
        printf("Submitted %u values\n", submitted_count);
        printf("%d %% %d = %d\n", submitted_count, print_step, (submitted_count % print_step));

    }
    
    cvr->value_size = random_value_gen(cvr->value);
    
    //Set creation timestamp
    gettimeofday(&cvr->creation_time, NULL);
    
    //Set expiration as creation+timeout
    sum_timevals(&cvr->expire_time, &cvr->creation_time, &values_timeout);
    
    //Send the value to proposers and return immediately
    pax_submit_nonblock(psh, cvr->value, cvr->value_size);
    
}

int is_expired(struct timeval * deadline, struct timeval * time_now) {
    if(time_now->tv_sec > deadline->tv_sec) {
        return 1;
    }

    if(time_now->tv_sec == deadline->tv_sec &&
            time_now->tv_usec >= deadline->tv_usec) {
        return 1;
    }
    return 0;
}

static void
set_timeout_check() {
    event_add(&cl_periodic_event, &cl_periodic_interval);
}

static void
cl_periodic_timeout_check(int fd, short event, void *arg) {
    
    struct timeval time_now;
    gettimeofday(&time_now, NULL);

    // Iterate over the values submitted by this client
    unsigned int i;
    client_value_record * iter;
    for(i = 0; i < concurrent_values; i++) {
        iter = &values_table[i];

        if (is_expired(&iter->expire_time, &time_now)) {
            submit_old_value(iter);
        }    
    }
    
    //And set a timeout for calling this function again
    set_timeout_check();
    
    //Makes the compiler happy
    fd = fd;
    event = event;
    arg = arg;
}

//This is executed by the libevent/learner thread
//Before learner init returns
int cl_init() {
    
    psh = pax_submit_handle_init();
    if (psh == NULL) {
        printf("Client init failed [submit handle]\n");
        return -1;        
    }
    
    //Create table to store values submitted
    values_table = malloc(sizeof(client_value_record) * concurrent_values);
    if(values_table == NULL) {
        printf("Client init failed [malloc]\n");
        return -1;
    }
    
    if(wait_after_init > 0) {
        sleep(wait_after_init);
    }
    
    //Submit N new values
    unsigned int i;
    for(i = 0; i < concurrent_values; i++) {
        submit_new_value(&values_table[i]);
    }
    
    //And set a timeout to check expired ones,
    evtimer_set(&cl_periodic_event, cl_periodic_timeout_check, NULL);    
    set_timeout_check();
    
    return 0;
}

void cl_deliver(char* value, size_t val_size, iid_t iid, ballot_t ballot, int proposer) {

    delivered_count += 1;
    assert((int)iid == delivered_count);
    
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    
    // Iterate over the values submitted by this client
    unsigned int i;
    client_value_record * iter;
    for(i = 0; i < concurrent_values; i++) {
        iter = &values_table[i];
        if(val_size == iter->value_size && 
                memcmp(value, iter->value, val_size) == 0) {
            //Our value, submit a new one!
            save_latency_info(iter, &time_now);
            submit_new_value(iter);
            break;
        }    
    }
    
    //Makes the compiler happy
    iid = iid;
    ballot = ballot;
    proposer = proposer;
}

int main (int argc, char const *argv[]) {
    
    signal(SIGINT, handle_cltr_c);

    //Default timeout for values
    values_timeout.tv_sec = 5;
    values_timeout.tv_usec = 0;

    parse_args(argc, (char **)argv);
    print_benchmark_parameters();
    
    start_time = time(NULL);
    end_time = start_time + duration;
    
    
    //Default timeout check interval
    cl_periodic_interval.tv_sec = 3;
    cl_periodic_interval.tv_usec = 0;
    
    //Clear latency statistics
    min_latency.tv_sec = 999999;
    min_latency.tv_usec = 999999;
    max_latency.tv_sec = 0;
    max_latency.tv_usec = 0;
    aggregated_latency.tv_sec = 0;
    aggregated_latency.tv_usec = 0;
    
    if(learner_init(cl_deliver, cl_init) != 0) {
        printf("Failed to start the learner!\n");
        return -1;
    }
    
    //Wait until benchmark time expires, then exit
    while(!force_exit && time(NULL) < end_time) {
        sleep(1);
    }    
    
    printf("Total delivered:%u\n", delivered_count);
    printf("\tRate:%f\n", ((float)delivered_count/duration));
    printf("Total submitted:%u\n", submitted_count);
    printf("\tRate:%f\n", ((float)submitted_count/duration));
    printf("Timed-out values:%u\n", retried_count);

    double min_lat_ms = ((double)min_latency.tv_sec * 1000) +
        ((double)min_latency.tv_usec / 1000);
    double max_lat_ms = ((double)max_latency.tv_sec * 1000) +
        ((double)max_latency.tv_usec / 1000);
    double avg_lat_ms = (((double)aggregated_latency.tv_sec * 1000) +
        ((double)aggregated_latency.tv_usec / 1000))/aggregated_latency_count;
    
    printf("Latency - Avg:%.2f, Min:%.2f, Max:%.2f\n", 
        avg_lat_ms, min_lat_ms, max_lat_ms);
        
    if(sample_frequency != 0) {
        printf("Latency samples: \n");
        int i;
        for(i = 0; i < samples_count; i++) {
            printf("%.2f, ", samples[i]);
            if(i % 20 == 0) {
                printf("\n");
            }
        }
        printf("\n");
    }
    return 0;
}
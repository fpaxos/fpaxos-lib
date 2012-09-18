//This is a very simple failure detector
//Which has the responsibility to elect a leader among the proposer
//In this case alive_ping messages received are ignored (just printed)
//The leader is by default 0 and can be changed via prompt
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>


#include "libpaxos.h"
#include "paxos_udp.h"

short int current_leader = 0;
udp_send_buffer * to_proposers;
udp_receiver * for_oracle;
int announce_interval = 5;
pthread_mutex_t oracle_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t announce_thread;
pthread_t user_input_thread;
pthread_t get_pings_thread;

void oracle_lock() {
    pthread_mutex_lock(&oracle_mutex);
}

void oracle_unlock() {
    pthread_mutex_unlock(&oracle_mutex);
}

void init_oracle_state() {
    //Initialize state
}
void update_oracle_state(short int proposer_id, long unsigned int seq_num, struct timeval * current_time) {
    current_time = current_time;
    printf("Got a ping n:%lu from proposer %d \n", seq_num, proposer_id);
    // short int new_leader;
    //...update rules here...
    
    //Lock before updating current_leader!
    // oracle_lock();
    // current_leader = new_leader;
    // oracle_unlock();
}

void * broadcast_current_leader(void * arg) {
    arg = arg;
    while(1) {
        //Get current leader
        oracle_lock();
        short int leader = current_leader;        
        oracle_unlock();
        
        //Send it
        sendbuf_send_leader_announce(to_proposers, leader);
        
        //Sleep for a while
        sleep(announce_interval);
    }
    return NULL;
}

void * get_proposer_pings(void * arg) {
    arg = arg;
    struct timeval current_time;
    //Read the next message
    while(1) {
        int valid = udp_read_next_message(for_oracle);
        if (valid < 0) {
            printf("Dropping invalid proposer message\n");
            continue;
        }

    //The message is valid, take the appropriate action
    // based on the type
        paxos_msg * msg = (paxos_msg*) &for_oracle->recv_buffer;
        
        switch(msg->type) {
            case alive_ping: {
                alive_ping_msg * ap = (alive_ping_msg *) msg->data;
                gettimeofday(&current_time, NULL);
                update_oracle_state(ap->proposer_id, ap->sequence_number, &current_time);
            }
            break;

            default: {
                printf("Unknow msg type %d received by oracle\n", msg->type);
            }
        }
    }
    return NULL;
}

void * get_user_input(void * arg) {
    arg = arg;
    int proposer_id;
    printf("To force a leader, enter the proposer id [0...%d]\n",
        MAX_N_OF_PROPOSERS-1);

    while(1) {
        printf(">");
        if(scanf("%d", &proposer_id) != 1) {
            printf("Not a valid proposer ID\n");
            continue;
        } 
        
        if(proposer_id < 0 || proposer_id >= MAX_N_OF_PROPOSERS) {
            printf("Proposer IDs are in the range 0...%d\n",
                MAX_N_OF_PROPOSERS-1);
            continue;
        }
        
        oracle_lock();
        if(proposer_id == current_leader) {
            printf("Proposer %d is already leader\n", proposer_id);
            oracle_unlock();
            continue;
        }

        printf("Leader is now proposer %d\n", proposer_id);
        current_leader = proposer_id;
        
        oracle_unlock();
    }
    return NULL;
}

int main (int argc, char const *argv[])
{
    argc = argc;
    argv = argv;
    
    //Init oracle sender and receiver
    to_proposers = udp_sendbuf_new(PAXOS_ORACLE_NET);
    if(to_proposers == NULL) {
        printf("Error creating oracle->proposers network sender\n");
        return -1;
    }
    
    for_oracle = udp_receiver_blocking_new(PAXOS_PINGS_NET);
    if (for_oracle == NULL) {
        printf("Error creating proposers->oracle network receiver\n");
        return -1;
    }
    
    init_oracle_state();
    
    oracle_lock();

    // Starts 3 threads sharing a global lock
    //Thread 1: periodically send current leader
    pthread_create(&announce_thread, NULL, broadcast_current_leader, NULL);
    //Thread 2: Receive alive_ping from proposers
    pthread_create(&get_pings_thread, NULL, get_proposer_pings, NULL);    
    //Thread 3: Ask the user to force a particular leader
    pthread_create(&user_input_thread, NULL, get_user_input, NULL);

    oracle_unlock();
    
    while(1) {
        sleep(10);
    }
    return 0;
}
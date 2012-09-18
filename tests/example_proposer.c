#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "libpaxos.h"

void handle_cltr_c (int sig) {
	printf("Caught signal %d\n", sig);
    // acceptor_exit();
    exit(0);
}

int main (int argc, char const *argv[]) {

    signal(SIGINT, handle_cltr_c);
    
    if (argc != 2) {
        printf("This program takes exactly one argument: the proposer unique identifier\n");
        exit(1);
    }
    
    short int proposer_id = atoi(argv[1]);
    
    if (proposer_init(proposer_id) != 0) {
        printf("Could not start the proposer!\n");
        exit(1);
    }
    
    
    while(1) {
        //This thread does nothing...
        //But it can't terminate!
        sleep(1);
    }


    return 0;
}

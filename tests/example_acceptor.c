#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "libpaxos.h"

void handle_cltr_c (int sig) {
	printf("Caught signal %d\n", sig);
    acceptor_exit();
    exit(0);
}

int main (int argc, char const *argv[]) {

    signal(SIGINT, handle_cltr_c);
    
    if (argc != 3) {
        printf("Usage: %s id config\n", argv[0]);
        exit(1);
    }
    
    short int acceptor_id = atoi(argv[1]);
    
    if (acceptor_init(acceptor_id, argv[2]) != 0) {
        printf("Could not start the acceptor!\n");
    }
	
    return 0;
}

#include "config_reader.h"
#include "paxos_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

static int rate = 1; // values per sec

static struct bufferevent* 
do_connect(struct event_base* b, address* a)
{
	struct sockaddr_in sin;
	struct bufferevent* bev;
	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(a->address_string);
	sin.sin_port = htons(a->port);
	
	bev = bufferevent_socket_new(b, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_enable(bev, EV_WRITE);
	struct sockaddr* addr = (struct sockaddr*)&sin;
	if (bufferevent_socket_connect(bev, addr, sizeof(sin)) < 0) {
        bufferevent_free(bev);
        return NULL;
	}
	return bev;
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
	struct event_base* base;
    struct bufferevent* bev;
	
	signal(SIGINT, handle_cltr_c);
	
    if (argc != 3) {
        printf("Usage: %s config rate\n", argv[0]);
        exit(1);
    }
	
	rate = atoi(argv[2]);

	config* c = read_config(argv[1]);

	base = event_base_new();    
	bev = do_connect(base, &c->proposers[0]);
	
	while(1) {
		int i;
		for (i = 0; i < rate; ++i) {
			sendbuf_add_submit_val(bev, "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789", 100);
			event_base_dispatch(base);
		}
		sleep(1);
	}
    return 0;
}

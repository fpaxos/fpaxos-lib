/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <evpaxos.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


static void
event_callback(struct bufferevent* bev, short events, void* arg)
{
    if (events & BEV_EVENT_CONNECTED) {
		printf("Connected\n");
    } else if (events & BEV_EVENT_ERROR) {
        printf("%s\n", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    }
}

static struct bufferevent* 
connect_to_proposer(struct event_base* b, struct sockaddr* addr)
{
	struct bufferevent* bev;
	
	bev = bufferevent_socket_new(b, -1, BEV_OPT_CLOSE_ON_FREE);
	bufferevent_setcb(bev, NULL, NULL, event_callback, NULL);
	if (bufferevent_socket_connect(bev, addr, sizeof(struct sockaddr)) < 0) {
		bufferevent_free(bev);
		return NULL;
	}
	event_base_dispatch(b);
	return bev;
}

void
usage(const char* progname)
{
	printf("Usage: %s address:port rate\n", progname);
	exit(1);
}

int
main (int argc, char const *argv[])
{
	int rate;
	struct event_base* base;
	struct bufferevent* bev;
	struct sockaddr address;
	int address_len = sizeof(struct sockaddr);
	
	if (argc != 3)
		usage(argv[0]);

	if (evutil_parse_sockaddr_port(argv[1], &address, &address_len) == -1)
		usage(argv[0]);
	rate = atoi(argv[2]);
	
	base = event_base_new();    
	bev = connect_to_proposer(base, &address);
	
	char value[] = "hello!";
	int len = strlen(value) + 1;
	
	while(1) {
		int i;
		for (i = 0; i < rate; ++i) {
			paxos_submit(bev, value, len);
			event_base_dispatch(base);
		}
		sleep(1);
	}
	return 0;
}

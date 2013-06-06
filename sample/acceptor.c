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


#include <stdlib.h>
#include <stdio.h>
#include <evpaxos.h>
#include <signal.h>

void
handle_sigint(int sig, short ev, void* arg)
{
	struct event_base* base = arg;
	printf("Caught signal %d\n", sig);
	event_base_loopexit(base, NULL);
}

int 
main (int argc, char const *argv[])
{	
	int id;
	struct event* sig;
	struct evacceptor* acc;
	struct event_base* base;
	
	if (argc != 3) {
		printf("Usage: %s id config\n", argv[0]);
		exit(0);
	}
	
	base = event_base_new();
	
	id = atoi(argv[1]);
	acc = evacceptor_init(id, argv[2], base);
	if (acc == NULL) {
		printf("Could not start the acceptor\n");
		return 0;
	}
	
	sig = evsignal_new(base, SIGINT, handle_sigint, base);
	evsignal_add(sig, NULL);
	
	event_base_dispatch(base);
	evacceptor_exit(acc);
	
	return 1;
}

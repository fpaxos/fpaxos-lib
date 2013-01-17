#ifndef _LIBPAXOS_PRIV_H_
#define _LIBPAXOS_PRIV_H_

#include <time.h>
#include <sys/time.h>

#include "libpaxos.h"
#include "paxos_config.h"

#include "libpaxos_messages.h"

typedef accept_ack acceptor_record;

/*** LOGGING MACROS ***/

#define VRB 1
#define DBG 3

#define LOG(L, S) if(VERBOSITY_LEVEL >= L) {\
   printf("[%s] ", __func__) ;\
   printf S ;\
}


typedef struct address_t {
	char* address_string;
	int port;
} address;

/*** MISC MACROS ***/

#endif /* _LIBPAXOS_PRIV_H_ */

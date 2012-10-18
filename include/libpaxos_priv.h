#ifndef _LIBPAXOS_PRIV_H_
#define _LIBPAXOS_PRIV_H_
// #include <netinet/in.h>
// #include <arpa/inet.h>
// 
#include <time.h>
#include <sys/time.h>

#include "libpaxos.h"
#include "paxos_config.h"

#include "libpaxos_messages.h"
#include "values_handler.h"


typedef accept_ack acceptor_record;


/*** MALLOC DEBUGGING MACROs ***/

#ifdef PAXOS_DEBUG_MALLOC
void * paxos_debug_malloc(size_t size, char* file, int line);
void paxos_debug_free(void* p, char* file, int line);
#define PAX_MALLOC(x) paxos_debug_malloc(x, __FILE__, __LINE__)
#define PAX_FREE(x) paxos_debug_free(x, __FILE__, __LINE__)
#else
void* paxos_normal_malloc(size_t size);
#define PAX_MALLOC(x) paxos_normal_malloc(x);
#define PAX_FREE(x) free(x)
#endif

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

// Since this lib compiles with -Wextra, unused parameters throw
// a warning, adding a USELESS_PARAMETER(variable_name) removes the message
#define UNUSED_ARG(V) (V = V);


#endif /* _LIBPAXOS_PRIV_H_ */
// 

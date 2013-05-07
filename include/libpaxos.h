#ifndef _LIBPAXOS_H_
#define _LIBPAXOS_H_

#include <sys/types.h>
#include <stdint.h>

/* 
    The maximum size that can be submitted by a client.
    Set MAX_UDP_MSG_SIZE in config file to reflect your network MTU.
    Max packet size minus largest header possible
    (should be accept_ack_batch+accept_ack, around 30 bytes)
*/
#define PAXOS_MAX_VALUE_SIZE (MAX_UDP_MSG_SIZE - 40)

/* 
    Alias for instance identificator and ballot number.
*/
typedef unsigned int ballot_t;
typedef uint32_t iid_t;


#endif /* _LIBPAXOS_H_ */

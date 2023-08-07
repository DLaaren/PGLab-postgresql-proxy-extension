/* contrib/proxy/proxy.h */

#ifndef PROXY_H
#define PROXY_H

#define BUFFER_SIZE 1024
#include "storage/lwlock.h"

typedef struct Channel {
    struct pollfd *front_fd;            /* client */
    struct pollfd *back_fd;             /* postgres */
    char front_to_back[BUFFER_SIZE];
    char back_to_front[BUFFER_SIZE];
    int bytes_received_from_front;
    int bytes_received_from_back;
    int port;
    LWLock lock;
} Channel;

/*
* Init list channels struct in shared memory.
* Use in ShmemInitStruct function "proxy_channels" as name arguement to
* access channels list.
*/
List *
init_proxy_channels();

/*
 * Runs proxy
 */
extern void
run_proxy();

extern void
shutdown_proxy();

#endif /* PROXY_H */
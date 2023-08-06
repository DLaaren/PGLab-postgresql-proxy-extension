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
} Channel;

/*
* ProxyChannels is a struct in postgres shared memory.
* Use in ShmemInitStruct function "proxy_channels" as name arguement to
* access channels list.
*/
typedef struct {
    List *channels;
    LWLock lock;
} ProxyChannels;

/*
* Init proxy channels struct
*/
ProxyChannels *
init_proxy_channels();

/*
 * Runs proxy
 */
extern void
run_proxy();

extern void
shutdown_proxy();

#endif /* PROXY_H */
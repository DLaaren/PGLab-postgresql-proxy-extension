/* contrib/proxy/proxy_manager.h */

#ifndef PROXY_MANAGER_H
#define PROXY_MANAGER_H

#include "postgres.h"
#include "storage/lwlock.h"
#include "nodes/pg_list.h"

/*
 * Proxy toxy setting struct.
 */
typedef struct {
    List *channels;
    LWLock lock;
} ProxyChannels;

extern ProxyChannels *
init_proxy_channels();

extern Datum 
set_speed(PG_FUNCTION_ARGS);

#endif /* PROXY_MANAGER_H */
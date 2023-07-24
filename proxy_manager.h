/*
 * contrib/proxy/proxy_manager.h
 */

#ifndef PROXY_MANAGER_H
#define PROXY_MANAGER_H

#include "postgres.h"
#include "storage/lwlock.h"
/*
 * Proxy toxy setting struct.
 */
typedef struct {
    int speed;
    LWLock lock;
} ProxySettings;

extern void
init_proxy_settings(ProxySettings*);

extern Datum set_speed(PG_FUNCTION_ARGS);

#endif /* PROXY_MANAGER_H */
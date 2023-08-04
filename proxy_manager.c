/* contrib/proxy/proxy_manager.c */

#include "postgres.h"

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "postmaster/interrupt.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "storage/lwlock.h"
#include "utils/backend_status.h"
#include "unistd.h"
#include "utils/wait_event.h"

#include "proxy_manager.h"

#define SIZE 25

PG_FUNCTION_INFO_V1(set_speed);

ProxyChannels *
init_proxy_channels() {
    ProxyChannels *proxy_settings;
    bool found;
    char shmem_name[SIZE] = { 0 };
    sprintf(shmem_name, "proxy_settings_%d", MyProcPid);
    proxy_settings = (ProxyChannels *) ShmemInitStruct(shmem_name, 
                                                     sizeof(ProxyChannels), 
                                                     &found);
    if (!found) {
        LWLockInitialize(&proxy_settings->lock, 1);
        proxy_settings->channels = NIL;
    } 
    return proxy_settings;
}

Datum
set_speed(PG_FUNCTION_ARGS)
{
    ProxyChannels *proxy_channels;
    if (PG_ARGISNULL(0)) {
        elog(ERROR, "could not override param.");
        PG_RETURN_VOID();
    }

    ProxyChannels *proxy_settings = init_proxy_channels();
    
    LWLockAcquire(&proxy_channels->lock, LW_EXCLUSIVE);
    elog(INFO, "proxy server speed change...");
    
    CHECK_FOR_INTERRUPTS();
    LWLockRelease(&proxy_channels->lock);
    
    PG_RETURN_VOID();
}

Datum
get_speed(PG_FUNCTION_ARGS)
{   
    ProxyChannels *proxy_settings = init_proxy_settings(&proxy_settings);
}

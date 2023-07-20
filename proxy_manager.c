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

void init_proxy_settings(ProxySettings *proxy_settings) {
    bool found;
    proxy_settings = (ProxySettings*)ShmemInitStruct("proxy_settings", 
                                                    sizeof(ProxySettings), 
                                                    &found);
    if (!found) {
        LWLockInitialize(&proxy_settings->lock, 1);
    } 
}


ProxySettings proxy_settings;

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(start_test_manager);

Datum
set_speed(PG_FUNCTION_ARGS)
{
    if (PG_ARGISNULL(0)) {
        elog(ERROR, "could not override param.");
        PG_RETURN_VOID();
    }

    init_proxy_settings(&proxy_settings);
    
    LWLockAcquire(&proxy_settings.lock, LW_EXCLUSIVE);
    elog(INFO, "proxy server speed change...");
    proxy_settings.speed = PG_GETARG_INT32(0);
    CHECK_FOR_INTERRUPTS();
    LWLockRelease(&proxy_settings.lock);
    elog(INFO, "proxy server speed changed to %d", proxy_settings.speed);
    PG_RETURN_VOID();
}


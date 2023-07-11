/*
 *  module description 
 *
 *  change default postgres_server port in configure file named "postgresql.conf" 
 */

#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"

#include "proxy.h"

PG_MODULE_MAGIC;

void _PG_init(void);

PGDLLEXPORT void proxy_main(Datum main_arg);

void
proxy_main(Datum main_arg)
{
/*
 *  Unblocking signals from postgres processes
 */
    BackgroundWorkerUnblockSignals();

/*
 *  Starting proxy server
 */
    run_proxy();
}

void 
_PG_init(void)
{
    elog(LOG, "proxy_server_bgw has began working.");

    BackgroundWorker proxy_bgw;

    memset(&proxy_bgw, 0, sizeof(proxy_bgw));
    proxy_bgw.bgw_flags = BGWORKER_SHMEM_ACCESS;
    proxy_bgw.bgw_start_time = BgWorkerStart_RecoveryFinished;
    proxy_bgw.bgw_restart_time = BGW_NEVER_RESTART;
    sprintf(proxy_bgw.bgw_library_name, "proxy");
    sprintf(proxy_bgw.bgw_function_name, "proxy_main");
    sprintf(proxy_bgw.bgw_name, "proxy_server_bgw");
    sprintf(proxy_bgw.bgw_type, "proxy_server");

    RegisterBackgroundWorker(&proxy_bgw);
    elog(LOG, "proxy_server_bgw has been registered");
}
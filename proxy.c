#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"

#include "proxy.h"

PG_MODULE_MAGIC;

void _PG_init(void);

PGDLLEXPORT void proxy_main(Datum main_arg);

void
proxy_main(Datum main_arg) {
    BackgroundWorkerUnblockSignals();

    run_proxy();
}

void 
_PG_init(void) {
    elog(LOG, "Proxy has began to work.");

    BackgroundWorker worker;

    memset(&worker, 0, sizeof(worker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = BGW_NEVER_RESTART;
    sprintf(worker.bgw_library_name, "proxy");
    sprintf(worker.bgw_function_name, "proxy_main");
    sprintf(worker.bgw_name, "proxy server worker");
    sprintf(worker.bgw_type, "proxy");

    elog(LOG, "register proxy server");
    RegisterBackgroundWorker(&worker);
}
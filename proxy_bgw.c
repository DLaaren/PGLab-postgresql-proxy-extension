/*
 * contrib/proxy/proxy_bgw.c
 */

/*
 *  module description 
 *
 *  change default postgres_server port in configure file named "postgresql.conf" 
 */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "utils/guc.h"

#include "proxy.h"

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

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

static char *proxy_addr;
static int proxy_port;
static int max_channels;

void 
_PG_init(void)
{
    if (IsUnderPostmaster)
		ereport(ERROR,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("proxy must be loaded via shared_preload_libraries")));
    
    proxy_addr = calloc(16, sizeof(char));

    /*
     *
     */
    DefineCustomStringVariable("proxy.proxy_addr", 
                               "", 
                               NULL, 
                               &proxy_addr, 
                               "localhost", 
                               PGC_POSTMASTER, 
                               GUC_NOT_IN_SAMPLE, 
                               NULL, 
                               NULL, 
                               NULL);

    /* 
     *
     */
    DefineCustomIntVariable("proxy.proxy_port",
                            "",
                            NULL,
                            &proxy_port,
                            5432,
                            0,
                            65353,
                            PGC_POSTMASTER,
                            GUC_NOT_IN_SAMPLE,
                            NULL,
                            NULL,
                            NULL);

    /*
     *
     */
    DefineCustomIntVariable("proxy.max_channels", 
                            "", 
                            NULL, 
                            &max_channels, 
                            15, 
                            0, 
                            100000, 
                            PGC_POSTMASTER, 
                            GUC_NOT_IN_SAMPLE, 
                            NULL, 
                            NULL, 
                            NULL);

    MarkGUCPrefixReserved("proxy");

    BackgroundWorker proxy_bgw;
    
    elog(LOG, "Proxy server bgw has been registered");

    memset(&proxy_bgw, 0, sizeof(proxy_bgw));
    proxy_bgw.bgw_flags = BGWORKER_SHMEM_ACCESS;
    proxy_bgw.bgw_start_time = BgWorkerStart_RecoveryFinished;
    proxy_bgw.bgw_restart_time = BGW_NEVER_RESTART;
    sprintf(proxy_bgw.bgw_library_name, "proxy");
    sprintf(proxy_bgw.bgw_function_name, "proxy_main");
    sprintf(proxy_bgw.bgw_name, "proxy_server_bgw");
    sprintf(proxy_bgw.bgw_type, "proxy_server");

    RegisterBackgroundWorker(&proxy_bgw);
}

void
_PG_fini()
{
    free(proxy_addr);
}
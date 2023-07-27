/* contrib/proxy/proxy_bgw.c */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "utils/guc.h"

#include <signal.h>
#include "proxy.h"

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

PGDLLEXPORT void proxy_main(Datum main_arg);

static void
sigint_handler(SIGNAL_ARGS)
{
    elog(INFO, "proxy received SIGINT");
    shutdown_proxy();
}

static void
sigquit_handler(SIGNAL_ARGS)
{
    elog(INFO, "proxy received SIGQUIT");
    shutdown_proxy();
}

static void
sigterm_handler(SIGNAL_ARGS)
{
    elog(INFO, "proxy received SIGTERM");
    shutdown_proxy();
}

void
proxy_main(Datum main_arg)
{
    struct sigaction act_sigint = {0}, act_sigquit = {0}, act_sigterm = {0};
    act_sigint.sa_handler = &sigint_handler;
    act_sigquit.sa_handler = &sigquit_handler;
    act_sigterm.sa_handler = &sigterm_handler;
    if (sigaction(SIGINT, &act_sigint, NULL) == -1 ||
        sigaction(SIGQUIT, &act_sigquit, NULL) == -1 ||
        sigaction(SIGTERM, &act_sigterm, NULL) == -1)
    {
        elog(ERROR, "sigaction() error");
        exit(1);
    }

    BackgroundWorkerUnblockSignals();
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


    DefineCustomStringVariable("proxy.listening_address", 
                               "This variable defines the proxy listening socket IPv4 address ", 
                               NULL, 
                               &proxy_addr, 
                               "127.0.0.1", 
                               PGC_POSTMASTER, 
                               GUC_NOT_IN_SAMPLE, 
                               NULL, 
                               NULL, 
                               NULL);

    DefineCustomIntVariable("proxy.port",
                            "This variable defines the proxy listening socket port",
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

    DefineCustomIntVariable("proxy.max_channels", 
                            "This variable defines the maximum possible channels", 
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
    memset(&proxy_bgw, 0, sizeof(proxy_bgw));

    elog(LOG, "Proxy server bgw has been registered");

    proxy_bgw.bgw_flags = BGWORKER_SHMEM_ACCESS;
    proxy_bgw.bgw_start_time = BgWorkerStart_RecoveryFinished;
    proxy_bgw.bgw_restart_time = BGW_DEFAULT_RESTART_INTERVAL;
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
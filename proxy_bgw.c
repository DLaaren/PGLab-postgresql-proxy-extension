/* contrib/proxy/proxy_bgw.c */

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "utils/guc.h"

#include <stdlib.h>
#include <signal.h>
#include "proxy.h"
#include "proxy_manager.h"

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

PGDLLEXPORT void proxy_main(Datum main_arg);

static int max_nodes;
static char **arr_listening_socket_addrs;
static int *arr_listening_socket_ports;
static char **arr_node_addrs;

static void
free_arrs()
{
    for (int node_idx = 1; node_idx < max_nodes; node_idx++)
    {
        free(arr_listening_socket_addrs[node_idx]);
        free(arr_node_addrs[node_idx]);
    }
    free(arr_listening_socket_addrs);
    free(arr_listening_socket_ports);
    free(arr_node_addrs);
}

static void
sigint_handler(SIGNAL_ARGS)
{
    elog(LOG, "proxy received SIGINT");
    shutdown_proxy();
    free_arrs();
    exit(2);
}

static void
sigquit_handler(SIGNAL_ARGS)
{
    elog(LOG, "proxy received SIGQUIT");
    // shutdown_proxy();
    exit(2);
}

static void
sigterm_handler(SIGNAL_ARGS)
{
    elog(LOG, "proxy received SIGTERM");
    shutdown_proxy();
    free_arrs();
    exit(2);
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
    ProxySettings proxy_settings;
    init_proxy_settings(&proxy_settings);

    BackgroundWorkerUnblockSignals();
    run_proxy();
}

void 
_PG_init(void)
{
    if (IsUnderPostmaster)
		ereport(FATAL,
				(errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE),
				 errmsg("proxy must be loaded via shared_preload_libraries")));
    
    DefineCustomIntVariable("proxy.max_nodes",
                            "Defines how many nodes can be connected to proxy",
                            NULL,
                            &max_nodes,
                            1,
                            0,
                            10000,
                            PGC_POSTMASTER,
                            GUC_NOT_IN_SAMPLE, /* the value can not be changed after the server start */
                            NULL,
                            NULL,
                            NULL
                            );

    arr_listening_socket_addrs = calloc(max_nodes + 1, sizeof(char*));
    arr_listening_socket_ports = calloc(max_nodes + 1, sizeof(int));
    arr_node_addrs = calloc(max_nodes + 1, sizeof(char*));
    for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
    {
        char node_name[20] = {0};
        sprintf(node_name, "proxy.node%d", node_idx); 

        char sock_addr_str[40] = {0};
        sprintf(sock_addr_str, "%s_listening_socket_addr", node_name);
        char *socket_addr = calloc(16, sizeof(char));
        arr_listening_socket_addrs[node_idx] = socket_addr;
        DefineCustomStringVariable(sock_addr_str,
                                   NULL,
                                   NULL,
                                   &(arr_listening_socket_addrs[node_idx]),
                                   "localhost",
                                   PGC_POSTMASTER,
                                   GUC_NOT_IN_SAMPLE,
                                   NULL,
                                   NULL,
                                   NULL);

        char sock_port_str[40] = {0};
        sprintf(sock_port_str, "%s_listening_socket_port", node_name);
        DefineCustomIntVariable(sock_port_str,
                                NULL,
                                NULL,
                                &(arr_listening_socket_ports[node_idx]),
                                15001,
                                0,
                                65535, 
                                PGC_POSTMASTER, 
                                GUC_NOT_IN_SAMPLE, 
                                NULL, 
                                NULL, 
                                NULL);

        char node_addr_str[40] = {0};
        sprintf(node_addr_str, "%s_addr", node_name);
        char *node_addr = calloc(16, sizeof(char));
        arr_node_addrs[node_idx] = node_addr;
        DefineCustomStringVariable(node_addr_str, 
                                   NULL, 
                                   NULL, 
                                   &(arr_node_addrs[node_idx]), 
                                   "localhost", 
                                   PGC_POSTMASTER, 
                                   GUC_NOT_IN_SAMPLE, 
                                   NULL, 
                                   NULL, 
                                   NULL);
    }

    /* DefineCustomIntVariable("proxy.max_channels", 
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
                            NULL); */

    MarkGUCPrefixReserved("proxy");

    /* shared memory declaration && allocation */

    /* ... */

    BackgroundWorker proxy_bgw;
    memset(&proxy_bgw, 0, sizeof(proxy_bgw));

    proxy_bgw.bgw_flags = BGWORKER_SHMEM_ACCESS;
    proxy_bgw.bgw_start_time = BgWorkerStart_RecoveryFinished;
    proxy_bgw.bgw_restart_time = BGW_DEFAULT_RESTART_INTERVAL;
    sprintf(proxy_bgw.bgw_library_name, "proxy");
    sprintf(proxy_bgw.bgw_function_name, "proxy_main");
    sprintf(proxy_bgw.bgw_name, "proxy_server_bgw");
    sprintf(proxy_bgw.bgw_type, "proxy_server");

    RegisterBackgroundWorker(&proxy_bgw);
    elog(LOG, "Proxy server bgw has been registered");
}

void
_PG_fini()
{
    shutdown_proxy();
    free_arrs();
}
/* contrib/proxy/proxy.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "postgres.h"
#include "fmgr.h"
#include "c.h"
#include "postmaster/postmaster.h"
#include "utils/guc.h"
#include "nodes/pg_list.h"

#include "proxy.h"
#include "proxy_log.h"
// #include "proxy_manager.h"

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define IS_LOCALHOST(addr) ((strcmp(addr, "localhost") == 0) ? ("127.0.0.1") : (addr))

#define POSTGRES_ADDR (IS_LOCALHOST(ListenAddresses)) // ((strcmp(ListenAddresses, "localhost") == 0) ? ("127.0.0.1") : (ListenAddresses))
#define POSTGRES_CURR_PORT PostPortNumber
#define POSTGRES_SOCKET_DIR Unix_socket_directories

static int max_nodes;
static char **arr_listening_socket_addrs;
static int *arr_listening_socket_ports;
static char **arr_node_addrs;
static int max_connections;

static int *arr_proxy_sockets_fds;
static List *channels = NIL;
static size_t fds_len;
static struct pollfd *fds;

static int find_conf_vars();
static int open_proxy_listening_sockets();

static int accept_connection(int proxy_socket, int node_idx);
static int connect_postgres_server();

static int read_data_front_to_back(Channel *curr_channel);
static int read_data_back_to_front(Channel *curr_channel);
static int write_data_front_to_back(Channel *curr_channel);
static int write_data_back_to_front(Channel *curr_channel);

static List *create_channel(int postgres_socket, int client_socket);
static void delete_channel(Channel *curr_channel);

void 
run_proxy()
{ 
    elog(LOG, "proxy server is getting config variables...");

    if (find_conf_vars() == -1)
    {
        elog(FATAL, "error while getting config variables");
        shutdown_proxy();
        exit(1);
    }

    if (open_proxy_listening_sockets() == -1)
    {
        elog(FATAL, "error while opening proxy listening sockets");
        shutdown_proxy();
        exit(1);
    }

    elog(LOG, "proxy server is running and waiting for connections...");

    ListCell *cell = NULL;
    int postgres_socket, client_socket;

    // channels = init_proxy_channels()->channels;
    
    fds_len = max_connections * 2 * max_nodes + max_nodes + 1; /* max free idx of array of fds */
    fds = malloc(fds_len * sizeof(struct pollfd));
    memset(fds, -1, fds_len * sizeof(struct pollfd));

    /* Setting all listening sockets to fds array */
    for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
    {
        fds[node_idx].fd = arr_proxy_sockets_fds[node_idx];
        fds[node_idx].events = POLLIN;
    }

    /*-------------------------- main loop --------------------------*/
    for (;;)
    {
        if (poll(fds, fds_len, -1) == -1)
        {
            elog(FATAL, "poll() error");
            shutdown_proxy();
            exit(1);
        }

        // printf("fds array : ");
        // for (int i = 0; i < fds_len; i++)
        // {
        //     printf("%d ", fds[i].fd);
        // } printf("\n");

        /* accepting new connections and creating new channel */
        for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
        {
            if ((fds[node_idx].revents & POLLIN))
            { 
                client_socket = accept_connection(fds[node_idx].fd, node_idx);
                if (client_socket == -1)
                {
                    elog(ERROR, "connection with node%d failed", node_idx);
                    continue;
                }

                postgres_socket = connect_postgres_server();
                if (postgres_socket == -1)
                {
                    elog(ERROR, "connection with postgres server for node%d failed", node_idx);
                    continue;
                }

                channels = create_channel(postgres_socket, client_socket, arr_listening_socket_ports[node_idx]);
            }
        }

        /* check all list with channels if the channel is ready for reading/writing */
        foreach(cell, channels)
        {
            Channel *curr_channel = lfirst(cell);

            struct pollfd *curr_fd = curr_channel->front_fd;
            if (curr_fd->revents & POLLIN)
            {   
                if (read_data_front_to_back(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel);
                    continue;
                }
            }

            curr_fd = curr_channel->back_fd;
            if (curr_fd->revents & POLLOUT)
            {
                if (write_data_front_to_back(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel);
                    continue;
                }

            }

             if (curr_fd->revents & POLLIN)
            {   
                if (read_data_back_to_front(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel);
                    continue;
                }
            }

            curr_fd = curr_channel->front_fd;
            if (curr_fd->revents & POLLOUT)
            {
                if (write_data_back_to_front(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel);
                    continue; 
                }
            }
        }
    }
    /*-------------------------- main loop --------------------------*/

    shutdown_proxy();
}


int
find_conf_vars()
{
    if (parse_int(GetConfigOption("proxy.max_nodes", true, false), &max_nodes, 0, NULL) == false)
    {
        elog(ERROR, "GetConfigOption() error --- cannot get max_nodes value");
        return -1;
    }

    if (parse_int(GetConfigOption("proxy.max_connections", true, false), &max_connections, 0, NULL) == false)
    {
        elog(ERROR, "GetConfigOption() error --- cannot get max_connections value");
        return -1;
    }

    arr_listening_socket_addrs = calloc(max_nodes+1, sizeof(char*));
    arr_listening_socket_ports = calloc(max_nodes+1, sizeof(int));
    arr_node_addrs = calloc(max_nodes+1, sizeof(char*));

    for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
    {
        char node_name[20] = {0};
        sprintf(node_name, "proxy.node%d", node_idx); 

        char sock_addr_str[40] = {0};
        sprintf(sock_addr_str, "%s_listening_socket_addr", node_name);
        arr_listening_socket_addrs[node_idx] = IS_LOCALHOST(GetConfigOption(sock_addr_str, true, false)); // strcmp(GetConfigOption(sock_addr_str, true, false), "localhost") == 0 ? "127.0.0.1" : GetConfigOption(sock_addr_str, true, false);
        if (arr_listening_socket_addrs[node_idx] == NULL)
        {
            elog(ERROR, "GetConfigOption() for node%d error --- cannot get proxy listening socket address", node_idx);
            return -1;
        }

        char sock_port_str[40] = {0};
        sprintf(sock_port_str, "%s_listening_socket_port", node_name);
        if(parse_int(GetConfigOption(sock_port_str, true, false), &(arr_listening_socket_ports[node_idx]), 0, NULL) == false)
        {
            elog(ERROR, "GetConfigOption() for node%d error --- cannot get proxy listening socket port", node_idx);
            return -1;
        }

        char node_addr_str[40] = {0};
        sprintf(node_addr_str, "%s_addr", node_name);
        arr_node_addrs[node_idx] = IS_LOCALHOST(GetConfigOption(node_addr_str, true, false));// strcmp(GetConfigOption(node_addr_str, true, false), "localhost") == 0 ? "127.0.0.1" : GetConfigOption(node_addr_str, true, false);
        if (arr_node_addrs[node_idx] == NULL)
        {
            elog(ERROR, "GetConfigOption() for node%d error --- cannot get node%d address", node_idx, node_idx);
            return -1;
        }
    }
    return 0;
}


int 
open_proxy_listening_sockets()
{
    arr_proxy_sockets_fds = calloc(max_nodes + 1, sizeof(int));

    for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
    {
        if ((arr_proxy_sockets_fds[node_idx] = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            elog(ERROR, "socket() for node%d error --- cannot create proxy listening socket", node_idx);
            return -1;
        }

        int opt;
        if (setsockopt(arr_proxy_sockets_fds[node_idx], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        {
            elog(ERROR, "setsockopt() for node%d error --- cannot set flags on proxy listening socket", node_idx);
            return -1;
        }

        struct sockaddr_in proxy_address;
        proxy_address.sin_family = AF_INET;
        if (inet_pton(AF_INET, arr_listening_socket_addrs[node_idx], &(proxy_address.sin_addr)) != 1)
        {
            elog(ERROR, "inet_pton() for node%d error --- cannot convert proxy listening socket address", node_idx);
            return -1;
        }
        proxy_address.sin_port = htons(arr_listening_socket_ports[node_idx]);

        if (bind(arr_proxy_sockets_fds[node_idx], (struct sockaddr *)&proxy_address, sizeof(proxy_address)) == -1)
        {
            if (errno == EADDRINUSE)
            {
                elog(ERROR, "port %d for proxy is already in use, try another or kill the process using this port", arr_listening_socket_ports[node_idx]);
                return -1;
            }
            elog(ERROR, "bind() for node%d error --- cannot bind proxy listening socket address", node_idx);
            return -1;
        }

        if (listen(arr_proxy_sockets_fds[node_idx], max_connections) == -1)
        {
            elog(ERROR, "listen() for node%d error --- cannot preapare proxy listening socket to accept connections", node_idx);
            return -1;
        }

        elog(LOG, "listening on %s:%d", arr_listening_socket_addrs[node_idx], arr_listening_socket_ports[node_idx]);
    }
    return 0;
}


int
accept_connection(int proxy_socket, int node_idx) 
{
    struct sockaddr_in client_address;
    socklen_t client_len;
    client_address.sin_family = AF_INET;
    if (inet_pton(AF_INET, arr_node_addrs[node_idx], &(client_address.sin_addr)) != 1)
    {
        elog(ERROR, "inet_pton() for node%d error --- cannot convert node%d address", node_idx, node_idx);
        return -1;
    } 
    // client_address.sin_port = htons(proxy_port); /* ? */
    client_len = sizeof(client_address);

    int client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        elog(ERROR, "accept() for node%d error --- cannot accept connection from node%d", node_idx, node_idx);
        return -1;
    }

    elog(LOG, "new connection from node%d: %s:%d", 
               node_idx,
               inet_ntoa(client_address.sin_addr),
               ntohs(client_address.sin_port));
    return client_socket;
}


int
connect_postgres_server()
{
    int postgres_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (postgres_socket == -1)
    {
        elog(ERROR, "socket() error --- cannot create socket for postgres server connection");
        return -1;
    }

    struct sockaddr_in postgres_address;
    postgres_address.sin_family = AF_INET;
    int err;
    if ((err = inet_pton(AF_INET, POSTGRES_ADDR, &(postgres_address.sin_addr))) != 1)
    {
        if (err == 0) 
        {
            elog(WARNING, "postgres addr is NULL -- trying connect to UNIX-socket");
            close(postgres_socket);
            return connect_postgres_server_using_unix_socket();
        }
        else {
            elog(LOG, "inet_pton() error --- cannot convert postgres server address");
            return -1;
        }
    }
    postgres_address.sin_port = htons(POSTGRES_CURR_PORT);

    if (connect(postgres_socket, (struct sockaddr *)&postgres_address, sizeof(postgres_address)) == -1)
    {
        elog(LOG, "connect() error --- cannot connect to postgres server");
        return -1;
    }

    elog(LOG, "proxy has connected to postgres server successfully");
    return postgres_socket;
}

int
connect_postgres_server_using_unix_socket()
{
    int postgres_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un postgress_unix_socket_address;
    postgress_unix_socket_address.sun_family = AF_UNIX;
    sprintf(postgress_unix_socket_address.sun_path, "%s/.s.PGSQL.%d", POSTGRES_SOCKET_DIR, POSTGRES_CURR_PORT);

    if (connect(postgres_socket, (struct sockaddr *)&postgress_unix_socket_address, sizeof(postgress_unix_socket_address)) == -1)
    {
        elog(LOG, "connect() error --- cannot connect to postgres server via unix socket");
        return -1;
    }

    elog(LOG, "proxy has connected to postgres server successfully");
    return postgres_socket;
}


List *
create_channel(int postgres_socket, int client_socket, int port)
{
    Channel *new_channel = (Channel *)calloc(1, sizeof(Channel));

    bool postgres_fd_inserted = false;
    bool client_fd_inserted = false;
    for (int idx = max_nodes + 1; idx < fds_len; idx++)
    {
        if (fds[idx].fd == -1 && client_fd_inserted == false)
        {
            fds[idx].fd = client_socket;
            fds[idx].events = POLLIN | POLLOUT;
            new_channel->front_fd = &(fds[idx]);
            client_fd_inserted = true;
        }
        if (fds[idx].fd == -1 && postgres_fd_inserted == false)
        {
            fds[idx].fd = postgres_socket;
            fds[idx].events = POLLIN | POLLOUT;
            new_channel->back_fd = &(fds[idx]);
            postgres_fd_inserted = true;
        }
        if (client_fd_inserted == true && postgres_fd_inserted == true)
        {
            new_channel->port = port;
            break;
        }
    }

    if (client_fd_inserted == false || postgres_fd_inserted == false)
    {
        elog(WARNING, "attemp to connect --- max nodes has been already connected");
        if (new_channel->front_fd->fd != -1)
        {
            close(new_channel->front_fd->fd);
        }
        if (new_channel->back_fd->fd != 1) {
            close(new_channel->back_fd->fd);
        }
        free(new_channel);
        return channels;
    }

    channels = lappend(channels, new_channel);
    elog(LOG, "new channel has been created");
    return channels;
}


int
read_data_front_to_back(Channel *curr_channel)
{
    curr_channel->bytes_received_from_front = read(curr_channel->front_fd->fd, curr_channel->front_to_back, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_front == -1)
    {
        elog(ERROR, "error while reading from front to back");
        return -1;
    }
    if (curr_channel->bytes_received_from_front == 0)
    {
        elog(WARNING, "connection has been lost");
        return -1;
    }
    // elog(LOG, "read from front (fd %d) to back %d bytes", curr_channel->front_fd->fd, curr_channel->bytes_received_from_front);
    return 0;
}

int
read_data_back_to_front(Channel *curr_channel)
{
    curr_channel->bytes_received_from_back = read(curr_channel->back_fd->fd, curr_channel->back_to_front, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_back == -1)
    {
        elog(ERROR, "error while reading from back to front");
        return -1;
    }
    if (curr_channel->bytes_received_from_back == 0)
    {
        elog(WARNING, "connection has been lost");
        return -1;
    } 
    // elog(LOG, "read from back (fd %d) to front %d bytes", curr_channel->back_fd->fd, curr_channel->bytes_received_from_back);
    return 0;
}

int
write_data_front_to_back(Channel *curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_front > 0) 
    {
        bytes_written = write(curr_channel->back_fd->fd, curr_channel->front_to_back, curr_channel->bytes_received_from_front);
        // elog(LOG, "write to back (fd %d) %d bytes", curr_channel->back_fd->fd, bytes_written);

        memset(curr_channel->front_to_back, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_front = 0;
    }
    if (bytes_written == -1)
    {
        elog(ERROR, "error while writing from front to back");
        return -1;
    }
    return 0;
}

int
write_data_back_to_front(Channel *curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_back > 0)
    {
        bytes_written = write(curr_channel->front_fd->fd, curr_channel->back_to_front, curr_channel->bytes_received_from_back);
        // elog(LOG, "write to front (fd %d) %d bytes", curr_channel->front_fd->fd, bytes_written);

        memset(curr_channel->back_to_front, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_back = 0;
    }
    if (bytes_written == -1)
    {
        elog(LOG, "error while writing from back to front");
        return -1;
    }
    return 0;
}


void
delete_channel(Channel *curr_channel)
{
    close(curr_channel->front_fd->fd);
    close(curr_channel->back_fd->fd);

    curr_channel->front_fd->fd = -1;
    curr_channel->back_fd->fd = -1;
    elog(WARNING, "channel has been deleted");
}

void shutdown_proxy()
{
    for (int node_idx = 0; node_idx < fds_len; node_idx++) {
        if (fds[node_idx].fd != -1) {
            close(fds[node_idx].fd);
        }
    }
    free(arr_listening_socket_addrs);
    free(arr_listening_socket_ports);
    free(arr_node_addrs);
    free(arr_proxy_sockets_fds);

    list_free(channels);

    elog(LOG, "proxy server is shutting down...");
}

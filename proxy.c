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
// #include "proxy_log.h"
// #include "proxy_manager.h"

#define MAX_CHANNELS 1

#define POSTGRES_ADDR ((strcmp(ListenAddresses, "localhost") == 0) ? ("127.0.0.1") : (ListenAddresses))
#define POSTGRES_CURR_PORT PostPortNumber

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static int max_nodes;
static char **arr_listening_socket_addrs;
static int *arr_listening_socket_ports;
static char **arr_node_addrs;

static int
read_data_front_to_back(Channel *curr_channel)
{
    curr_channel->bytes_received_from_front = read(curr_channel->front_fd->fd, curr_channel->front_to_back, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_front == -1)
    {
        elog(LOG, "error while reading from front (fd %d) to back : ", curr_channel->front_fd->fd);
        return -1;
    }
    if (curr_channel->bytes_received_from_front == 0)
    {
        elog(LOG, "connection has been lost");
        return -1;
    }
    elog(LOG, "read from front (fd %d) to back %d bytes", curr_channel->front_fd->fd, curr_channel->bytes_received_from_front);
    return 0;
}

static int
read_data_back_to_front(Channel *curr_channel)
{
    curr_channel->bytes_received_from_back = read(curr_channel->back_fd->fd, curr_channel->back_to_front, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_back == -1)
    {
        elog(LOG, "error while reading from back (fd %d) to front", curr_channel->back_fd->fd);
        return -1;
    }
    if (curr_channel->bytes_received_from_back == 0)
    {
        elog(LOG, "connection has been lost");
        return -1;
    } 
    elog(LOG, "read from back (fd %d) to front %d bytes", curr_channel->back_fd->fd, curr_channel->bytes_received_from_back);
    return 0;
}

static int
write_data_front_to_back(Channel *curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_front > 0) 
    {
        bytes_written = write(curr_channel->back_fd->fd, curr_channel->front_to_back, curr_channel->bytes_received_from_front);
        elog(LOG, "write to back (fd %d) %d bytes", curr_channel->back_fd->fd, bytes_written);

        memset(curr_channel->front_to_back, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_front = 0;
    }
    if (bytes_written == -1)
    {
        elog(LOG, "error while writing from front to back (fd %d)", curr_channel->back_fd->fd);
        return -1;
    }
    return 0;
}

static int
write_data_back_to_front(Channel *curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_back > 0)
    {
        bytes_written = write(curr_channel->front_fd->fd, curr_channel->back_to_front, curr_channel->bytes_received_from_back);
        elog(LOG, "write to front (fd %d) %d bytes", curr_channel->front_fd->fd, bytes_written);

        memset(curr_channel->back_to_front, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_back = 0;
    }
    if (bytes_written == -1)
    {
        elog(LOG, "error while writing from back to front (fd %d)", curr_channel->front_fd->fd);
        return -1;
    }
    return 0;
}

static int
connect_postgres_server()
{
    int postgres_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (postgres_socket == -1)
    {
        elog(LOG, "error while creating postgres socket");
        return -1;
    }

    struct sockaddr_in postgres_address;
    postgres_address.sin_family = AF_INET;
    if (inet_pton(AF_INET, POSTGRES_ADDR, &(postgres_address.sin_addr)) != 1)
    {
        elog(LOG, "error while converting postgres address");
        return -1;
    }
    postgres_address.sin_port = htons(POSTGRES_CURR_PORT);

    if (connect(postgres_socket, (struct sockaddr *)&postgres_address, sizeof(postgres_address)) == -1)
    {
        elog(LOG, "error while connecting to postgres server");
        return -1;
    }

    elog(LOG, "proxy has connected to postgres server successfully (fd %d)", postgres_socket);
    return postgres_socket;
}

static int
accept_connection(int proxy_socket, int node_idx) 
{
    struct sockaddr_in client_address;
    socklen_t client_len;
    client_address.sin_family = AF_INET;
    if (inet_pton(AF_INET, arr_node_addrs[node_idx], &(client_address.sin_addr)) != 1)
    {
        elog(LOG, "error while converting client address with index %d", node_idx);
        return -1;
    } 
    // client_address.sin_port = htons(proxy_port); /* здеся будет определённый порт */
    client_len = sizeof(client_address);

    int client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        elog(LOG, "error while accepting a connection from front");
        return -1;
    }

    elog(LOG, "new connection from client: %s:%d (fd %d)",
                           inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), client_socket);
    return client_socket;
}

static List *
create_channel(List *channels, struct pollfd *fds, int fds_len, int postgres_socket, int client_socket)
{
    Channel *new_channel = (Channel *)calloc(1, sizeof(Channel));

    bool postgres_fd_inserted = false;
    bool client_fd_inserted = false;
    for (size_t idx = 1; idx < fds_len; idx++)
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
            break;
        }
    }

    if (client_fd_inserted == false || postgres_fd_inserted == false)
    {
        free(new_channel);
        return channels;
    }
    channels = lappend(channels, new_channel);
    elog(LOG, "new channel has been created");

    return channels;
}

static void
delete_channel(Channel *curr_channel, struct pollfd *fds)
{
    close(curr_channel->front_fd->fd);
    close(curr_channel->back_fd->fd);

    curr_channel->front_fd->fd = -1;
    curr_channel->back_fd->fd = -1;
    elog(LOG, "channel has been deleted");
}

void shutdown_proxy(struct pollfd *fds, size_t fds_len, List *channels)
{
    elog(LOG, "closing all fds...");

    for (int i = 0; i < fds_len; i++) {
        if (fds[i].fd != -1) {
            close(fds[i].fd);
        }
    }

    list_free(channels);

    elog(LOG, "proxy server is shutting down...");
}

/* TODO */
    /* add check for same addresses and ports of postgres and proxy */
    /* check pg_indent */
    /* tap tests */
    /* several bgws -- try to start twi-three -- think how to differ them */

static int
find_conf_vars()
{
    if (parse_int(GetConfigOption("proxy.max_nodes", true, false), &max_nodes, 0, NULL) == false)
    {
        elog(LOG, "could not get max_nodes config variable");
        return -1;
    }

    for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
    {
        char node_name[20] = {0};
        sprintf(node_name, "proxy.node%d", node_idx); 

        char sock_addr_str[40] = {0};
        sprintf(sock_addr_str, "%s_listening_socket_addr", node_name);
        arr_listening_socket_addrs[node_idx] = GetConfigOption(sock_addr_str, true, false);
        if (arr_listening_socket_addrs[node_idx] == NULL)
        {
            // error
            return -1;
        }

        char sock_port_str[40] = {0};
        sprintf(sock_port_str, "%s_listening_socket_port", node_name);
        if(parse_int(GetConfigOption(sock_port_str, true, false), &(arr_listening_socket_ports[node_idx]), 0, NULL) == false)
        {
            // error
            return -1;
        }

        char node_addr_str[40] = {0};
        sprintf(node_addr_str, "%s_addr", node_name);
        arr_node_addrs[node_idx] = GetConfigOption(node_addr_str, true, false);
        if (arr_node_addrs[node_idx] == NULL)
        {
            // error
            return -1;
        }

        printf("%s %d %s\n", arr_listening_socket_addrs[node_idx], arr_listening_socket_ports[node_idx], arr_node_addrs[node_idx]);
    }
    return 0;
}

void 
run_proxy()
{ 
    if (find_conf_vars() == -1)
    {
        elog(LOG, "could not get config variables");
        exit(1);
    }
    /*------------- opening all sockets -------------*/

    printf("max_nodes %d \n\n", max_nodes);

    int *arr_proxy_sockets = calloc(max_nodes, sizeof(int));

    for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
    {
        arr_proxy_sockets[node_idx] = socket(AF_INET, SOCK_STREAM, 0);
        if (arr_proxy_sockets[node_idx] == -1)
        {
            elog(LOG, "error while creating proxy socket with index %d", node_idx);
            exit(1);
        }

        if (setsockopt(arr_proxy_sockets[node_idx], SOL_SOCKET, SO_REUSEADDR, NULL, 0) == -1)
        {
            elog(LOG, "can not set options for proxy socket with index %d", node_idx);
            close(arr_proxy_sockets[node_idx]);
            exit(1);
        }

        struct sockaddr_in proxy_address;
        proxy_address.sin_family = AF_INET;
        if (inet_pton(AF_INET, arr_listening_socket_addrs[node_idx], &(proxy_address.sin_addr)) != 1)
        {
            elog(LOG, "error while converting proxy address");
            close(arr_proxy_sockets[node_idx]);
            exit(1);
        }
        proxy_address.sin_port = htons(arr_listening_socket_ports[node_idx]);

        if (bind(arr_proxy_sockets[node_idx], (struct sockaddr *)&proxy_address, sizeof(proxy_address)) == -1)
        {
            if (errno == EADDRINUSE)
            {
                elog(LOG, "port %d for proxy is already in use, try another or kill the process using this port", arr_listening_socket_ports[node_idx]);
            }
            elog(LOG, "error while binding proxy socket");
            close(arr_proxy_sockets[node_idx]);
            exit(1);
        }

        if (listen(arr_proxy_sockets[node_idx], MAX_CHANNELS) == -1)
        {
            elog(LOG, "error while listening from proxy socket");
            close(arr_proxy_sockets[node_idx]);
            exit(1);
        }

        printf("\nlisten on %s:%d\n\n", arr_listening_socket_addrs[node_idx], arr_listening_socket_ports[node_idx]);
    }
    /*------------- opening all sockets -------------*/

    elog(LOG, "proxy server is running and waiting for connections...");

    List *channels = NIL;
    ListCell *cell = NULL;
    int postgres_socket, client_socket;
    
    size_t fds_len = MAX_CHANNELS * 2 + max_nodes + 1; /* max free idx of array of fds */
    struct pollfd *fds = malloc(fds_len * sizeof(struct pollfd));
    memset(fds, -1, fds_len * sizeof(struct pollfd));

    for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
    {
        fds[node_idx].fd = arr_proxy_sockets[node_idx];
        fds[node_idx].fd = POLLIN;
    }

    for (;;)
    {
        int err = poll(fds, fds_len, -1);
        if (err == -1)
        {
            elog(LOG, "error during poll()");
            break;
        }

        /* if any proxy socket is ready to accept connection then accept it and create new channel */
        for (int node_idx = 1; node_idx <= max_nodes; node_idx++)
        {
            if ((fds[node_idx].revents & POLLIN))
            { 
                client_socket = accept_connection(fds[node_idx].fd, node_idx);
                if (client_socket == -1)
                {
                    break;
                }
                postgres_socket = connect_postgres_server();
                if (postgres_socket == -1)
                {
                    break;
                }
                channels = create_channel(channels, fds, fds_len, postgres_socket, client_socket);
            }
        }

        foreach(cell, channels)
        {
            Channel *curr_channel = lfirst(cell); /* Macros to access the data values within List cells. */
            struct pollfd *curr_fd = curr_channel->front_fd;

            /* if front is ready to send data then read data to buffer */
            if (curr_fd->revents & POLLIN)
            {   
                if (read_data_front_to_back(curr_channel) == -1)
                {
                    elog(LOG, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel, fds);
                    continue;
                }
            }

            curr_fd = curr_channel->back_fd;

            /* if back is ready to get data then write data from buffer to back socket */
            if (curr_fd->revents & POLLOUT)
            {
                if (write_data_front_to_back(curr_channel) == -1)
                {
                    elog(LOG, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel, fds);
                    continue;
                }

            }

            /* if back is ready to send data then read data to buffer */
             if (curr_fd->revents & POLLIN)
            {   
                if (read_data_back_to_front(curr_channel) == -1)
                {
                    elog(LOG, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel, fds);
                    continue;
                }
            }

            curr_fd = curr_channel->front_fd;

            /* if front is ready to get data then write data from buffer to front socket */
            if (curr_fd->revents & POLLOUT)
            {
                if (write_data_back_to_front(curr_channel) == -1)
                {
                    elog(LOG, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel, fds);
                    continue; 
                }
            }
        }
    }

    shutdown_proxy(fds, fds_len, channels);
}

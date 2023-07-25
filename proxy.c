#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#include "postgres.h"
#include "fmgr.h"
#include "c.h"
#include "postmaster/postmaster.h"
#include "utils/guc.h"
#include "nodes/pg_list.h"

#include "proxy.h"
#include "proxy_log.h"
// #include "proxy_manager.h"

#define POSTGRES_ADDR ListenAddresses
#define POSTGRES_CURR_PORT PostPortNumber
// #define PROXY_ADDR "localhost"
// #define PROXY_PORT 5432
// #define MAX_CHANNELS 10

static char *PROXY_ADDR;
static int PROXY_PORT;
static int MAX_CHANNELS;

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static int
read_data_front_to_back(Channel *curr_channel)
{
    curr_channel->bytes_received_from_front = read(curr_channel->front_fd->fd, curr_channel->front_to_back, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_front == -1)
    {
        elog(LOG_ERROR, "error while reading from front (fd %d) to back", curr_channel->front_fd->fd);
        return -1;
    }
    if (curr_channel->bytes_received_from_front == 0)
    {
        elog(LOG_INFO, "connection has been lost");
        return -1;
    }
    elog(LOG_INFO, "read from front (fd %d) to back %d bytes", curr_channel->front_fd->fd, curr_channel->bytes_received_from_front);
    return 0;
}

static int
read_data_back_to_front(Channel *curr_channel)
{
    curr_channel->bytes_received_from_back = read(curr_channel->back_fd->fd, curr_channel->back_to_front, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_back == -1)
    {
        elog(LOG_ERROR, "error while reading from back (fd %d) to front", curr_channel->back_fd->fd);
        return -1;
    }
    if (curr_channel->bytes_received_from_back == 0)
    {
        elog(LOG_INFO, "connection has been lost");
        return -1;
    } 
    elog(LOG_INFO, "read from back (fd %d) to front %d bytes", curr_channel->back_fd->fd, curr_channel->bytes_received_from_back);
    return 0;
}

static int
write_data_front_to_back(Channel *curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_front > 0) 
    {
        bytes_written = write(curr_channel->back_fd->fd, curr_channel->front_to_back, curr_channel->bytes_received_from_front);
        elog(LOG_INFO, "write to back (fd %d) %d bytes", curr_channel->back_fd->fd, bytes_written);

        memset(curr_channel->front_to_back, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_front = 0;
    }
    if (bytes_written == -1)
    {
        elog(LOG_ERROR, "error while writing from front to back (fd %d)", curr_channel->back_fd->fd);
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
        elog(LOG_INFO, "write to front (fd %d) %d bytes", curr_channel->front_fd->fd, bytes_written);

        memset(curr_channel->back_to_front, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_back = 0;
    }
    if (bytes_written == -1)
    {
        elog(LOG_ERROR, "error while writing from back to front (fd %d)", curr_channel->front_fd->fd);
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
        elog(LOG_ERROR, "error while creating postgres socket");
        return -1;
    }

    struct sockaddr_in postgres_server;
    postgres_server.sin_family = AF_INET;
    postgres_server.sin_addr.s_addr = inet_addr(POSTGRES_ADDR);
    postgres_server.sin_port = htons(POSTGRES_CURR_PORT);

    if (connect(postgres_socket, (struct sockaddr *)&postgres_server, sizeof(postgres_server)) == -1)
    {
        elog(LOG_ERROR, "error while binding postgres socket");
        return -1;
    }

    elog(LOG_INFO, "proxy has connected to postgres server successfully (fd %d)", postgres_socket);
    return postgres_socket;
}


static int
accept_connection(int proxy_socket) 
{
    struct sockaddr_in client_address;
    socklen_t client_len;
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(POSTGRES_CURR_PORT);
    client_address.sin_addr.s_addr = inet_addr(POSTGRES_ADDR);
    client_len = sizeof(client_address);

    int client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        elog(LOG_ERROR, "error while accepting a connection from front");
        return -1;
    }

    elog(LOG_INFO, "new connection from client: %s:%d (fd %d)",
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

    // for (size_t idx = 0; idx < MAX_CHANNELS*2 + 1; idx++)
    // {
    //     printf("%d ", fds[idx].fd);
    // }
    // printf("\n\n");

    return channels;
}

static void
delete_channel(Channel *curr_channel, struct pollfd *fds)
{
    close(curr_channel->front_fd->fd);
    close(curr_channel->back_fd->fd);

    curr_channel->front_fd->fd = -1;
    curr_channel->back_fd->fd = -1;

    // for (size_t idx = 0; idx < MAX_CHANNELS*2 + 1; idx++)
    // {
    //     printf("%d ", fds[idx].fd);
    // }
    // printf("\n\n");
}

    /* background worker exited with exit code 1  ---  restart proxy in that way */
    /* signal handler check for interrupts and for exit */
    /* check pg_indent */

static void
set_conf_vars()
{
    ConfigVariable *list_of_vars = NULL;
    FILE *config = fopen("proxy.conf", "r");
    if (config == NULL)
    {
        //error
        return -1;
    }
    if (ParseConfigFp(config, "proxy.conf", 0, 0, &list_of_vars, NULL) == false)
    {
        // error
        return -1;
    }

    ConfigVariable *curr_var = list_of_vars;
    while (curr_var != NULL)
    {
        if (strcmp(curr_var->name, "proxy_addr") == 0)
        {
            PROXY_ADDR = calloc(16, sizeof(char));
            strcpy(PROXY_ADDR, curr_var->value);
        }
        if (strcmp(curr_var->name, "proxy_port") == 0)
        {
            PROXY_PORT = atoi(curr_var->value);
        }
        if (strcmp(curr_var->name, "max_channels") == 0)
        {
            MAX_CHANNELS = atoi(curr_var->value);
        }
    }

    FreeConfigVariables(list_of_vars);
}

void 
run_proxy()
{

    // printf("\n\nlisten address %s and postgres curr port %d\n\n", POSTGRES_ADDR, POSTGRES_CURR_PORT); 
    set_conf_vars();

    int proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket == -1)
    {
        elog(LOG_ERROR, "error while creating proxy socket");
        exit(1);
    }

    /* we will still try to bind to occupied address and port, but only if there is no listening socket binding to the address */
    int opt;
    if (setsockopt(proxy_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        elog(LOG_ERROR, "can not set options for proxy socket");
        close(proxy_socket);
        exit(1);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(PROXY_ADDR);
    server_address.sin_port = htons(PROXY_PORT);

    if (bind(proxy_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        if (errno == EADDRINUSE)
        {
            elog(LOG_ERROR, "port for proxy is already in use, try another or kill the process using this port");
        }
        elog(LOG_ERROR, "error while binding proxy socket");
        close(proxy_socket);
        exit(1);
    }

    if (listen(proxy_socket, MAX_CHANNELS) == -1)
    {
        elog(LOG_ERROR, "error while listening from proxy socket");
        close(proxy_socket);
        exit(1);
    }

    elog(LOG_INFO, "proxy server is running and waiting for connections...");


    List *channels = NIL;
    ListCell *cell = NULL;
    int postgres_socket, client_socket;
    
    size_t fds_len = MAX_CHANNELS * 2 + 1; /* max free idx of array of fds */
    struct pollfd *fds = malloc(fds_len * sizeof(struct pollfd)); /* 'cause we have 2 fds in one channel + 1 for proxy socket */
    memset(fds, -1, sizeof(fds));
    fds[0].fd = proxy_socket;
    fds[0].events = POLLIN;

    for (;;)
    {
        int err = poll(fds, fds_len, -1);
        if (err == -1)
        {
            elog(LOG_ERROR, "error during poll()");
            break;
        }

        /* if proxy socket is ready to accept connection then accept it and create channel */
        if ((fds[0].revents & POLLIN))
        { 
            client_socket = accept_connection(proxy_socket);
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
            elog(LOG_INFO, "new channel has been created");
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
                    elog(LOG_INFO, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
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
                    elog(LOG_INFO, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
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
                    elog(LOG_INFO, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
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
                    elog(LOG_INFO, "channel has been deleted (fds %d and %d)", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
                    channels = foreach_delete_current(channels, cell);
                    delete_channel(curr_channel, fds);
                    continue; 
                }
            }
        }
    }

    elog(LOG_INFO, "closing all fds...");
    for (int i = 1; i < fds_len; i++) {
        if (fds[i].fd != -1) {
            close(fds[i].fd);
        }
    }

    free(fds);
    free(PROXY_ADDR);

    close(proxy_socket);
    list_free(channels);

    elog(LOG_INFO, "proxy server is shutting down...");

}

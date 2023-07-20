#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#include "c.h"
#include "nodes/pg_list.h"

#include "proxy.h"
#include "proxy_log.h"
#include "proxy_manager.h"

#define BUFFER_SIZE 1024
#define MAX_CHANNELS 10
#define LOCALHOST_ADDR "127.0.0.1"
#define DEFAULT_POSTGRES_PORT 5432
#define POSTGRES_CURR_PORT 55432 /* LATER : (optional) think how to get this port number instead of typing it */

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct channel {
    struct pollfd *front_fd;                           /* client */
    struct pollfd *back_fd;                            /* postgres */
    char front_to_back[BUFFER_SIZE];
    char back_to_front[BUFFER_SIZE];
    int bytes_received_from_front;
    int bytes_received_from_back;
} channel;

static int
read_data_front_to_back(channel *curr_channel)
{
    curr_channel->bytes_received_from_front = read(curr_channel->front_fd->fd, curr_channel->front_to_back, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_front == -1)
    {
        log_write(LOG_ERROR, "error while reading from front to back");
        return -1;
    }
    if (curr_channel->bytes_received_from_front == 0)
    {
        log_write(LOG_INFO, "connection has been lost");
        return -1;
    } 
    return 0;
}

static int
read_data_back_to_front(channel *curr_channel)
{
    curr_channel->bytes_received_from_back = read(curr_channel->back_fd->fd, curr_channel->back_to_front, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_back == -1)
    {
        log_write(LOG_ERROR, "error while reading from back to front");
        return -1;
    }
    if (curr_channel->bytes_received_from_back == 0)
    {
        log_write(LOG_INFO, "connection has been lost");
        return -1;
    } 
    return 0;
}

static int
write_data_front_to_back(channel * curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_front > 0) 
    {
        bytes_written = write(curr_channel->back_fd->fd, curr_channel->front_to_back, curr_channel->bytes_received_from_front);
        memset(curr_channel->front_to_back, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_front = 0;
    }
    if (bytes_written == -1)
    {
        log_write(LOG_ERROR, "error while writing from front to back");
        return -1;
    }
    return 0;
}

static int
write_data_back_to_front(channel * curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_back > 0)
    {
        bytes_written = write(curr_channel->front_fd->fd, curr_channel->back_to_front, curr_channel->bytes_received_from_back);
        memset(curr_channel->back_to_front, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_back = 0;
    }
    if (bytes_written == -1)
    {
        log_write(LOG_ERROR, "error while writing from back to front");
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
        log_write(LOG_ERROR, "error while creating postgres socket");
        return -1;
    }

    struct sockaddr_in postgres_server;
    postgres_server.sin_family = AF_INET;
    postgres_server.sin_port = htons(POSTGRES_CURR_PORT);
    postgres_server.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);

    if (connect(postgres_socket, (struct sockaddr *)&postgres_server, sizeof(postgres_server)) == -1)
    {
        log_write(LOG_ERROR, "error while binding postgres socket");
        return -1;
    }

    log_write(LOG_INFO, "proxy has connected to postgres server successfully\n");
    return postgres_socket;
}


static int
accept_connection(int proxy_socket) 
{
    struct sockaddr_in client_address;
    socklen_t client_len;
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(POSTGRES_CURR_PORT);
    client_address.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);
    client_len = sizeof(client_address);

    int client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        log_write(LOG_ERROR, "error while accepting a connection from front");
        return -1;
    }

    log_write(INFO, "new connection from client: %s:%d",
                           inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    return client_socket;
}

static List *
create_channel(List *channels, struct pollfd *postgres_socket, struct pollfd *client_socket)
{
    channel *new_channel = (channel *)calloc(1, sizeof(channel));
    new_channel->back_fd = postgres_socket;
    new_channel->front_fd = client_socket;
    channels = lappend(channels, new_channel);
    return channels;
}

static size_t
delete_channel(channel *curr_channel, struct pollfd *fds, size_t fds_len)
{
    printf("closing :%d and %d\n\n", curr_channel->front_fd->fd, curr_channel->back_fd->fd);
    close(curr_channel->front_fd->fd);
    close(curr_channel->back_fd->fd);

    curr_channel->front_fd->fd = -1;
    curr_channel->back_fd->fd = -1;

    /* resizing array of fds */
    while (fds_len > 0 && fds[fds_len - 1].fd == -1)
    {
        fds_len--;
    }

    for (size_t i = 0; i < fds_len; i++)
    {
        while (fds[i].fd == -1)
        {
            memmove(fds + i, fds + i + 1, sizeof(struct pollfd) * (fds_len - 1 - i));
            fds_len--;
        }
    }

    printf("fds_len %ld\n", fds_len);
        for (size_t i = 0; i < fds_len; i++)
        {
            printf("%d ", fds[i].fd);
        }
        printf("\n\n");

    return fds_len;
}

    /* TODO :: write timeouts for read and write */
    /* SIGINT SIGEXIT and postgres' signals handler */
    /* add function to on/off logs */
    /* QUESTION :: should we retry write/read if there was error not just close connection */
    /* background worker exited with exit code 1  ---  restart proxy in that way*/
    /* TODO:: Спросить у Рутмана стоит ли переделать проксю на unix сокеты */
    /* problems with deleting channels --- выявила пока тестировала pgbench'ом */

void 
run_proxy()
{
    log_open();  

    int proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket == -1)
    {
        log_write(LOG_ERROR, "error while creating proxy socket");
        exit(1);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(DEFAULT_POSTGRES_PORT);

    if (bind(proxy_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        log_write(LOG_ERROR, "error while binding proxy socket");
        close(proxy_socket);
        exit(1);
    }

    if (listen(proxy_socket, MAX_CHANNELS) == -1)
    {
        log_write(LOG_ERROR, "error while listening from proxy socket");
        close(proxy_socket);
        exit(1);
    }

    log_write(LOG_INFO, "proxy server is running and waiting for connections...");


    List *channels = NIL;
    ListCell *cell = NULL;
    int postgres_socket, client_socket;
    
    struct pollfd fds[MAX_CHANNELS * 2 + 1]; /* 'cause we have 2 fds in one channel + 1 for proxy socket */
    size_t fds_len = 1; /* max free idx of array of fds */
    memset(fds, 0 , sizeof(fds));
    fds[0].fd = proxy_socket;
    fds[0].events = POLLIN;

    for (;;)
    {
        int err = poll(fds, fds_len, -1);
        if (err == -1)
        {
            log_write(LOG_ERROR, "error during poll()");
            goto finalyze;
        }

        /* if proxy socket is ready to accept connection then accept it and create channel */
        if ((fds[0].revents & POLLIN) && fds_len < MAX_CHANNELS * 2 + 1)
        { 
            client_socket = accept_connection(proxy_socket);
            if (client_socket == -1)
            {
                goto finalyze;
            }
            postgres_socket = connect_postgres_server();
            if (postgres_socket == -1)
            {
                goto finalyze;
            }
            fds[fds_len].fd = client_socket;
            fds[fds_len].events = POLLIN | POLLOUT;
            fds_len++;
            fds[fds_len].fd = postgres_socket;
            fds[fds_len].events = POLLIN | POLLOUT;
            fds_len++;
            channels = create_channel(channels, &(fds[fds_len - 2]), &(fds[fds_len - 1]));
            log_write(INFO, "new channel has been created\n");
        }

        foreach(cell, channels)
        {
            channel *curr_channel = lfirst(cell); /* Macros to access the data values within List cells. */
            struct pollfd *curr_fd = curr_channel->front_fd;

            /* if front is ready to send data then read data to buffer */
            if (curr_fd->revents & POLLIN)
            {   
                if (read_data_front_to_back(curr_channel) == -1)
                {
                    fds_len = delete_channel(curr_channel, fds, fds_len);
                    channels = foreach_delete_current(channels, cell);
                    log_write(LOG_WARNING, "channel has been deleted");
                    continue;
                }
            }

            curr_fd = curr_channel->back_fd;

            /* if back is ready to get data then write data from buffer to back socket */
            if (curr_fd->revents & POLLOUT)
            {
                if (write_data_front_to_back(curr_channel) == -1)
                {
                    delete_channel(curr_channel, fds, &fds_len);
                    channels = foreach_delete_current(channels, cell);
                    log_write(WARNING, "channel has been deleted");
                    continue;
                }

            }

            /* if back is ready to send data then read data to buffer */
             if (curr_fd->revents & POLLIN)
            {   
                if (read_data_back_to_front(curr_channel) == -1)
                {
                    fds_len = delete_channel(curr_channel, fds, fds_len);
                    channels = foreach_delete_current(channels, cell);
                    log_write(LOG_WARNING, "channel has been deleted");
                    continue;
                }
            }

            curr_fd = curr_channel->front_fd;

            /* if front is ready to get data then write data from buffer to front socket */
            if (curr_fd->revents & POLLOUT)
            {
                if (write_data_back_to_front(curr_channel) == -1)
                {
                    fds_len = delete_channel(curr_channel, fds, fds_len);
                    channels = foreach_delete_current(channels, cell);
                    log_write(LOG_WARNING, "channel has been deleted");
                    continue; 
                }
            }

            curr_fd = curr_channel->back_fd;

            /* if back is ready to get data then write data from buffer to back socket */
            if (curr_fd->revents & POLLOUT)
            {
                if (write_data_front_to_back(curr_channel) == -1)
                {
                    fds_len = delete_channel(curr_channel, fds, fds_len);
                    channels = foreach_delete_current(channels, cell);
                    log_write(WARNING, "channel has been deleted");
                    continue;
                }
            }
        }
    }

    /* sorry but there is doule for */
    finalyze:
    log_write(LOG_INFO, "closing all fds...");
    for (int i = 1; i < fds_len; i++) {
        if (fds[i].fd != -1) {
            close(fds[i].fd);
        }
    }

    close(proxy_socket);
    list_free(channels);

    log_write(LOG_INFO, "proxy server is shutting down...");

    log_close();
}

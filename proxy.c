#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#include "c.h"
#include "nodes/pg_list.h"

#include "proxy.h"
#include "proxy_log.h"

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define LOCALHOST_ADDR "127.0.0.1"
#define DEFAULT_POSTGRES_PORT 5432
#define POSTGRES_CURR_PORT 55432 /* LATER : (optional) think how to get this port number instead of typing it */

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct channel {
    int front_fd;                           /* client */
    int back_fd;                            /* postgres */
    char front_to_back[BUFFER_SIZE];
    char back_to_front[BUFFER_SIZE];
    int bytes_received_from_front;
    int bytes_received_from_back;
} channel;


static int
read_data_front_to_back(channel *curr_channel)
{
    curr_channel->bytes_received_from_front = read(curr_channel->front_fd, curr_channel->front_to_back, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_front == -1)
    {
        return -1;
    }
    return 0;
}

static int
read_data_back_to_front(channel *curr_channel)
{
    curr_channel->bytes_received_from_back = read(curr_channel->back_fd, curr_channel->back_to_front, BUFFER_SIZE);
    if (curr_channel->bytes_received_from_back == -1)
    {
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
        bytes_written = write(curr_channel->back_fd, curr_channel->front_to_back, curr_channel->bytes_received_from_front);
        // printf("write data to back: %d bytes %s\n", bytes_written, curr_channel->front_to_back);
        memset(curr_channel->front_to_back, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_front = 0;
    }
    return bytes_written;
}

static int
write_data_back_to_front(channel * curr_channel)
{
    int bytes_written = 0;
    if (curr_channel->bytes_received_from_back > 0)
    {
        bytes_written = write(curr_channel->front_fd, curr_channel->back_to_front, curr_channel->bytes_received_from_back);
        // printf("write data to front: %d bytes %s\n", bytes_written, curr_channel->back_to_front);
        memset(curr_channel->back_to_front, 0, BUFFER_SIZE);
        curr_channel->bytes_received_from_back = 0;
    }
    return bytes_written;
}


static int
connect_postgres_server()
{
    struct sockaddr_in postgres_server;

    int postgres_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (postgres_socket == -1)
    {
        printf("Proxy socket creating error\n");
        exit(1);
    }

    postgres_server.sin_family = AF_INET;
    postgres_server.sin_port = htons(POSTGRES_CURR_PORT);
    postgres_server.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);

    if (connect(postgres_socket, (struct sockaddr *)&postgres_server, sizeof(postgres_server)) == -1)
    {
        printf("Socket binding error\n");
        exit(1);
    }

    printf("Proxy has connected to postgres server\n");
    return postgres_socket;
}


static int
accept_connection(int proxy_socket) 
{
    int client_socket;
    struct sockaddr_in client_address;
    socklen_t client_len;

    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(POSTGRES_CURR_PORT);
    client_address.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);
    client_len = sizeof(client_address);

    client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        printf("Client connection accept error\n");
    }

    printf("New connection from client: %s:%d\n",
                           inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    return client_socket;
}

static List *
create_channel(List *channels, int postgres_socket, int client_socket)
{
    /* idk what's wrong with palloc */
    /*
     *  probably because palloc allocates memory in backend's memory for transactions 
     *  but i'm not sure
     */
    channel *new_channel = (channel *)calloc(1, sizeof(channel));
    new_channel->back_fd = postgres_socket;
    new_channel->front_fd = client_socket;
    channels = lappend(channels, new_channel);
    return channels;
}

void 
run_proxy()
{
    int proxy_socket;
    struct sockaddr_in server_address;

    log_open();  

    proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket == -1)
    {
        printf("Proxy socket creating error\n");
        exit(1);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(DEFAULT_POSTGRES_PORT);

    if (bind(proxy_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        printf("Proxy's socket binding error\n");
        exit(1);
    }

    if (listen(proxy_socket, MAX_CLIENTS) == -1)
    {
        printf("Proxy's socket listening error\n");
        exit(1);
    }

    printf("The proxy server is running. Waiting for connections...\n");


    List *channels = NIL; /* = (void *)0  */
    ListCell *cell = NULL;

    fd_set master_fds, read_fds, write_fds;
    int max_fd;
    int postgres_socket, client_socket;
    
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    FD_ZERO(&master_fds);
    FD_SET(proxy_socket, &master_fds);
    max_fd = proxy_socket;

    for (;;)
    {
        // printf("\n            new iteration\n");
        write_fds = master_fds;
        read_fds = master_fds;
        if (select(max_fd + 1, &read_fds, &write_fds, NULL, &tv) == -1)
        {
            printf("select() error\n");
            continue;
        }

        /* if proxy socket is ready to accept connection then accept it and create channel */
        if (FD_ISSET(proxy_socket, &read_fds))
        {
            client_socket = accept_connection(proxy_socket);
            postgres_socket = connect_postgres_server();
            max_fd = MAX(client_socket, postgres_socket);
            FD_SET(client_socket, &master_fds);
            FD_SET(postgres_socket, &master_fds);
            channels = create_channel(channels, postgres_socket, client_socket);
            printf("channel has been created\n");
        }

        foreach(cell, channels)
        {
            channel *curr_channel = lfirst(cell); /* Macros to access the data values within List cells. */
            
            int fd = curr_channel->front_fd;

            /* if front is ready to send data then read data to buffer */
            if (FD_ISSET(fd, &read_fds))
            {   
                if (read_data_front_to_back(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    printf("channel has been deleted 1\n");
                    // continue;
                    goto exit_loop;
                }
            }

            fd = curr_channel->back_fd;

            /* if back is ready to get data then write data from buffer to back socket */
            if (FD_ISSET(fd, &write_fds))
            {
                if (write_data_front_to_back(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    printf("channel has been deleted 2\n");
                    // continue;
                    goto exit_loop;
                }

            }

            fd = curr_channel->back_fd;

            /* if back is ready to send data then read data to buffer */
             if (FD_ISSET(fd, &read_fds))
            {   
                if (read_data_back_to_front(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    printf("channel has been deleted 3\n");
                    // continue;
                    goto exit_loop;
                }
            }

            fd = curr_channel->front_fd;
            /* if front is ready to get data then write data from buffer to front socket */
            if (FD_ISSET(fd, &write_fds))
            {
                if (write_data_back_to_front(curr_channel) == -1)
                {
                    channels = foreach_delete_current(channels, cell);
                    printf("channel has been deleted 4\n");
                    // continue; 
                    goto exit_loop;
                }

            }
        }
    }

    exit_loop : close(proxy_socket);
    list_free(channels);

    printf("All sockets were closed. Proxy server is shutting down...\n");

    log_close();
}

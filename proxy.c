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

typedef enum idk {
    FRONT_TO_BACK,
    BACK_TO_FRONT
} idk;

typedef struct channel {
    int front_fd;                           /* client */
    int back_fd;                            /* postgres */
    char front_to_back[BUFFER_SIZE];
    char back_to_front[BUFFER_SIZE];
    int bytes_received_from_client;
    int bytes_received_from_postgres;
} channel;


static int
write_data(channel *channel, int where_to_write)
{
    int bytes_written = 0;
    switch (where_to_write)
    {
        case FRONT_TO_BACK:
            if (channel->bytes_received_from_client > 0)
            bytes_written = write(channel->back_fd, channel->front_to_back, channel->bytes_received_from_client);
            break;

        case BACK_TO_FRONT:
            if (channel->bytes_received_from_postgres > 0)
            bytes_written = write(channel->front_fd, channel->back_to_front, channel->bytes_received_from_postgres);
            break;

        default:
            /* error */
            break;
    }
    if (bytes_written == -1)
    {
        return -1;
    }
}

static int
read_data(channel *channel, int where_to_read)
{
    switch (where_to_read)
    {
        case FRONT_TO_BACK:
            //memset(channel->front_to_back, 0, BUFFER_SIZE);
            channel->bytes_received_from_client = read(channel->front_fd, channel->front_to_back, BUFFER_SIZE);
            break;

        case BACK_TO_FRONT:
            //memset(channel->back_to_front, 0, BUFFER_SIZE);
            channel->bytes_received_from_postgres = read(channel->back_fd, channel->back_to_front, BUFFER_SIZE);
            break;

        default:
            /* error */
            break;
    }
    if (channel->bytes_received_from_client == -1 || channel->bytes_received_from_postgres == -1)
    {
        return -1;
    }
    return 0;
}


static int
connect_postgres_server()
{
    struct sockaddr_in postgres_server;

    int postgres_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (postgres_socket == -1)
    {
        log_write(ERROR, "Proxy socket creating error");
        exit(1);
    }

    postgres_server.sin_family = AF_INET;
    postgres_server.sin_port = htons(POSTGRES_CURR_PORT);
    postgres_server.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);

    if (connect(postgres_socket, (struct sockaddr *)&postgres_server, sizeof(postgres_server)) == -1)
    {
        log_write(ERROR, "Socket binding error");
        exit(1);
    }

    log_write(INFO, "Proxy has connected to postgres server");
    return postgres_socket;
}


static int
accept_connection(int proxy_socket) 
{
    struct sockaddr_in client_address;

    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(POSTGRES_CURR_PORT);
    client_address.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);
    socklen_t client_len = sizeof(client_address);

    int client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        log_write(ERROR, "Client connection accept error");
    }

    printf("New connection from client: %s:%d\n",
                           inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
    printf("Initiating connection between client and postgres server");
    return client_socket;
}

static List *
create_channel(List *channels, int postgres_socket, int client_socket) /* хз в чем была проблема но я всё пофиксила лол */
{
    /* idk what's wrong with palloc */
    /*
     *  probably because palloc allocates memory in backend's memory for transactions 
     *  but i'm not sure
     */
    channel *new_channel = (channel *)calloc(1, sizeof(channel));
    new_channel->back_fd = postgres_socket;
    new_channel->front_fd = client_socket;
    /*
     *  Append a pointer to the list. A pointer to the modified list is
     *  returned. Note that this function may or may not destructively
     *  value, rather than continuing to use the pointer passed as the
     *  first argument.
     */
    
    channels = lappend(channels, new_channel);
    printf("channel has been created\n");
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
        log_write(ERROR, "Proxy socket creating error");
        exit(1);
    }

    /* bullshit ---> proxy exits with code 1 because of non-blocking accept */
    //int proxy_socket_flags = fcntl(proxy_socket, F_GETFL);
    //fcntl(proxy_socket, F_SETFL, proxy_socket_flags | O_NONBLOCK);

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(DEFAULT_POSTGRES_PORT);

    if (bind(proxy_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        log_write(ERROR, "Proxy's socket binding error");
        exit(1);
    }

    if (listen(proxy_socket, MAX_CLIENTS) == -1)
    {
        log_write(ERROR, "Proxy's socket listening error");
        exit(1);
    }

    log_write(INFO, "The proxy server is running. Waiting for connections...");

    printf("proxy socket is listening\n");


    List *channels = NIL;
    const ListCell *cell = NULL;

    fd_set master_fds, read_fds, write_fds;
    int max_fd;
    int postgres_socket, client_socket;
    
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    FD_ZERO(&master_fds);
    FD_SET(proxy_socket, &master_fds);
    max_fd = proxy_socket;

    printf("before the main loop\n");

    /* нужно также смотреть какие каналы недействительны и закрывать дескрипторы */
    for (;;)
    {
        write_fds = master_fds;
        read_fds = master_fds;
        if (select(max_fd + 1, &read_fds, &write_fds, NULL, &tv) == -1)
        {
            log_write(ERROR, "select()");
            continue;
        }

        if (FD_ISSET(proxy_socket, &read_fds))
        {
            client_socket = accept_connection(proxy_socket); /* cpu usage 100000)o)0o0 %%& */ /* LATER :: FIX IT CAUSE OTHERWISE MY PC WILL DIE*/
            postgres_socket = connect_postgres_server();
            max_fd = MAX(client_socket, postgres_socket);
            FD_SET(client_socket, &master_fds);
            FD_SET(postgres_socket, &master_fds);
            channels = create_channel(channels, postgres_socket, client_socket);
        }

        foreach(cell, channels)
        {
            channel *curr_channel = lfirst(cell);

            int fd = curr_channel->front_fd;

            /* Если фронт сокет готов к чтению, читаем данные с него во фронт буфер */
            if (FD_ISSET(fd, &read_fds))
            {   
                if (read_data(curr_channel, FRONT_TO_BACK) == -1)
                    channels = foreach_delete_current(channels, cell);
                printf("read data from front\n");
                continue;
            }
            /* Если же он готов к записи, то пишем из фронт буфера и отправляем по сокету постгресу */
            if (FD_ISSET(fd, &write_fds))
            {
                if (write_data(curr_channel, FRONT_TO_BACK) == -1)
                    channels = foreach_delete_current(channels, cell);
                printf("write data to front\n");
                continue;
            }

            fd = curr_channel->back_fd;

            /* Если бэкенд сокет готов к чтению, читаем данные с него в бэкенд буфер */
             if (FD_ISSET(fd, &read_fds))
            {   
                if (read_data(curr_channel, BACK_TO_FRONT) == -1)
                    channels = foreach_delete_current(channels, cell);
                printf("read data from postgres\n");
                continue;
            }
            /* Если же он готов к записи, то пишем из бэкенд буфера и отправляем по сокету клиенту */
            if (FD_ISSET(fd, &write_fds))
            {
                if (write_data(curr_channel, BACK_TO_FRONT) == -1)
                    channels = foreach_delete_current(channels, cell); 
                printf("write data to postgres\n");
                continue;
            }
        }
    }

    close(proxy_socket);
    list_free(channels);

    log_write(INFO, "All sockets were closed. Proxy server is shutting down...");

    log_close();
}

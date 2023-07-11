#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "postmaster/postmaster.c"

/* 
 *  Я запрещаю вам использовать треды!!
 *  И посмотрите стиль кода модулей в постгресе пж
 * 
 *  $PGDATA="/var/lib/postgres/data"
 *  change default port in postgresql.conf for another one
 * 
 */

#include "proxy.h"

#define BUFFER_SIZE 4096
#define ADDR "127.0.0.1"
#define DEFAULT_POSTGRES_PORT 5432

void
handle_client(int server_socket, int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_recieved = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_recieved <= 0) {
        if (bytes_recieved == 0)
        {
            printf("Client has lost connection\n");
        } else
        {
            perror("Data reading error");
        }

        close(client_socket);
    } else
    {
        buffer[bytes_recieved] = '\0';
        printf("Recieved from client: %s\n", buffer);
        
        if (send(client_socket, "The message was recieved", 18, 0) == -1)
        {
            perror("Data sending error");
        }
    }
}

int 
find_postgres_server_port()
{
    return PostPortNumber;
}

void
connect_postgres_server()
{
    int postgres_server_port = find_postgres_server_port();
    
    /* connect */
}


/*
 *  Rethink _exit() in "ifs"
 */

void
accept_connection(int client_socket, int server_socket, struct sockaddr_in *client_address) 
{
    socklen_t client_len = sizeof(client_address);
    client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        perror("Connection accept error");
        exit(1);
    }
}

void 
run_proxy()
{
    /* TODO: create client and server structure for avoiding too many params in functions */
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    int client_address_size = sizeof(client_address);

    /* Creating proxy_server socket */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        /* add error to proxy_log */
        perror("Proxy socket creating error");
        exit(1);
    }

    /* Preparing proxy_server address */
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(DEFAULT_POSTGRES_PORT);

    /* Binding socket to proxy_server address */
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        /* add error to proxy_log */
        perror("Socket binding error");
        exit(1);
    }

    /* TODO: run the function in another process */
    connect_postgres_server();

    /* Listening connection */
    if (listen(server_socket, 10) == -1)
    {
        /* add error to proxy_log */
        perror("Connection listening error");
        exit(1);
    }

    /* do we really need it --- otherwise, add it in the proxy_log */
    printf("The proxy server is running. Waiting for connections...\n");

    /* 
     * We need to use two fd sets because select() systemcall modify read_fds
     * and we must update this fd set in each iteration
     */
    fd_set master_fds, read_fds;
    /* For first argument of select() */
    int max_fd;

    FD_ZERO(&master_fds);
    FD_SET(server_socket, &master_fds);
    max_fd = server_socket;
    char buffer[BUFFER_SIZE];
    /* Accepting connections and handling clients */
    while(1)
    {
        read_fds = master_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("Select calling error");
            exit(1);
        }

        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &read_fds))
            {
                if (fd == server_socket)
                {
                    accept_connection(client_socket, server_socket, &client_address);
                    if (client_socket > max_fd)
                    {
                        max_fd = client_socket;
                    }
                    FD_SET(client_socket, &master_fds);
                    elog(LOG, "New connection from client: %s:%d\n",
                            inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                }
                else {
                    handle_client(server_socket, client_socket);
                    FD_CLR(fd, &master_fds);
                }
            }
        }
    }

    close(server_socket);
}
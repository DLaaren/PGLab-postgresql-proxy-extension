#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include "proxy.h"
#include "proxy_log.h"

#define BUFFER_SIZE 4096
#define LOCALHOST_ADDR "127.0.0.1"
#define DEFAULT_POSTGRES_PORT 5432
#define POSTGRES_CURR_PORT 55432 /* LATER : (optional) think how to get this port number instead of typing it */

void
handle_client_data(int proxy_socket, int postgres_socket, int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_recieved = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_recieved <= 0)
    {
        /* FOR EGOR:: get rid of these printfs and add your logging */
        if (bytes_recieved == 0)
        {
            printf("Client has lost connection\n");
        } else
        {
            perror("Data reading error");
        }

    } else
    {
        buffer[bytes_recieved] = '\0';
        printf("Recieved from client: %s\n", buffer);
        
        /*if (send(client_socket, "The message was recieved", 18, 0) == -1)
        {
            perror("Data sending error");
        }*/
    }
    if (send(postgres_socket, buffer, bytes_recieved, 0) == -1)
    {
        perror("Data sending error");
    }
    printf("sent data to postgres :: %s\n", buffer);
}

void
handle_postgres_data(int proxy_socket, int postgres_socket, int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_recieved = recv(postgres_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_recieved <= 0)
    {
        /* FOR EGOR:: get rid of these printfs and add your logging */
        if (bytes_recieved == 0)
        {
            printf("Posgres has lost connection\n");
        } else
        {
            perror("Data reading error");
        }

    } else
    {
        buffer[bytes_recieved] = '\0';
        printf("Recieved from postgres: %s\n", buffer);
    }
    if (send(client_socket, buffer, bytes_recieved, 0) == -1)
    {
        perror("Data sending error");
    }
    printf("sent data to client :: %s\n", buffer);
}

int
connect_postgres_server()
{
    int postgres_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (postgres_socket == -1)
    {
        perror("Proxy socket creating error");
        exit(1);
    }

    struct sockaddr_in postgres_server;
    postgres_server.sin_family = AF_INET;
    postgres_server.sin_port = htons(POSTGRES_CURR_PORT);
    postgres_server.sin_addr.s_addr = inet_addr(LOCALHOST_ADDR);

    if (connect(postgres_socket, (struct sockaddr *)&postgres_server, sizeof(postgres_server)) == -1)
    {
        perror("Socket binding error");
        exit(1);
    }

    printf("proxy connected to postgres server\n");
    return postgres_socket;
}


/*
 *  Rethink _exit() in "ifs"
 */

int
accept_connection(int client_socket, int proxy_socket, struct sockaddr_in *client_address) 
{
    socklen_t client_len = sizeof(client_address);
    client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        perror("Connection accept error");
        return -1;
    }
    return client_socket;
}

void 
run_proxy()
{
    //log_open();   

    /* TODO: create client and server structure for avoiding too many params in functions */
    int proxy_socket, client_socket;
    struct sockaddr_in server_address, client_address;

    proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket == -1)
    {
        perror("Proxy socket creating error");
        exit(1);
    }

    int postgres_socket = connect_postgres_server(proxy_socket);


    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(DEFAULT_POSTGRES_PORT);

    if (bind(proxy_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        perror("Socket binding error");
        exit(1);
    }

    if (listen(proxy_socket, 10) == -1)
    {
        perror("Connection listening error");
        exit(1);
    }

    printf("The proxy server is running. Waiting for connections...\n");


    /* 
     * We need to use two fd sets because select() systemcall modify read_fds
     * and we must update this fd set in each iteration
     */
    fd_set master_fds, read_fds; /* read_fds --- ready to read fds */
    /* For first argument of select() */
    int max_fd;

    FD_ZERO(&master_fds);
    FD_SET(proxy_socket, &master_fds);
    max_fd = proxy_socket;
    char buffer[BUFFER_SIZE];
    /* Accepting connections and handling clients */
    client_socket = accept_connection(client_socket, proxy_socket, &client_address);

    for (;;)
    {

        /*read_fds = master_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("Select calling error");
            exit(1);
        }

        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &read_fds))
            {
                if (fd == proxy_socket)
                {
                    client_socket = accept_connection(client_socket, proxy_socket, &client_address);
                    if (client_socket > max_fd)
                    {
                        max_fd = client_socket;
                    }
                    FD_SET(client_socket, &master_fds);
                    /* заменить на лог Егора */
                    //elog(LOG, "New connection from client: %s:%d\n",
                            //inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
                /*} else
                {
                    handle_client_data(proxy_socket, postgres_socket, fd);
                    handle_postgres_data(proxy_socket, postgres_socket, fd);
                    FD_CLR(fd, &master_fds);
                }
            }
        }*/

        handle_client_data(proxy_socket, postgres_socket, client_socket);
        handle_postgres_data(proxy_socket, postgres_socket, client_socket);
    }

    close(proxy_socket);
    close(postgres_socket);
    close(client_socket);
}
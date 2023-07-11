#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

/* 
 *  Я запрещаю вам использовать треды!!
 *  И посмотрите стиль кода модулей в постгресе пж
 */

#include "proxy_server.h"

#define BUFFER_SIZE 4096
#define ADDR "127.0.0.1"
#define PORT 8080

void
handle_client(int server_socket, int client_socket)
{
    char buffer[BUFFER_SIZE];

    /* Recieving data from client */
    int bytes_received;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        /* add to server_log which data received */
        /* Sending reply to the client */
        if (send(server_socket, buffer, bytes_received, 0) == -1)
        {
            /* add error to proxy_log */
            perror("Client data sending error");
            close(client_socket);
        }
    }

    /* doing something with the buffer */
}


/*
 *  Rethink _exit() in "ifs"
 */

void 
run_proxy()
{
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
    server_address.sin_port = htons(PORT);

    /* Binding socket to proxy_server address */
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        /* add error to proxy_log */
        perror("Socket binding error");
        exit(1);
    }

    /* Listening connection */
    if (listen(server_socket, 10) == -1)
    {
        /* add error to proxy_log */
        perror("Connection listening error");
        exit(1);
    }

    /* do we really need it --- otherwise, add it in the proxy_log */
    printf("The proxy server is running. Waiting for connections...\n");

    /* Server loop */
    for (;;) 
    {
        /* Accepting connections */
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, (socklen_t *)&client_address_size);
        if (client_socket == -1)
        {
            /* add error to proxy_log */
            perror("Connection accepting error");
            exit(1);
        } 
        
        /* do we really need it --- otherwise, add it in the proxy_log */
        printf("Connection accepted.\n");
        
        handle_client(server_socket, client_socket);

        /* I think it is more reasonable to let server close client_socket in loop 
         * 'cauz something can go wrong in that function -- > socket wom't be closed 
         */
        close(client_socket);
    }

    close(server_socket);
}
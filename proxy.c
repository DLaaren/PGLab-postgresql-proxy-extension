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
handle_client_data(int postgres_socket, int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_recieved = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_recieved <= 0)
    {
        if (bytes_recieved == 0)
        {
            log_write(WARNING, "Client has lost connection");
        } else
        {
            log_write(ERROR, "Client's data reading error");
        }

    } else
    {
        log_write(INFO, "Recieved from client: %s\n", buffer);
    }
    if (send(postgres_socket, buffer, bytes_recieved, 0) == -1)
    {
        log_write(ERROR, "Data sending to postgres error");
    }
    log_write(INFO, "Sent data to postgres: %s\n", buffer);
}

void
handle_postgres_data(int postgres_socket, int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_recieved = recv(postgres_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_recieved <= 0)
    {
        if (bytes_recieved == 0)
        {
            log_write(WARNING, "Postgres has lost connection");
        } else
        {
            log_write(ERROR, "Postgres' data reading error");
        }

    } else
    {
        log_write(INFO, "Recieved from postgres: %s\n", buffer);
    }
    if (send(client_socket, buffer, bytes_recieved, 0) == -1)
    {
        log_write(ERROR, "Data sending to client error");
    }
    log_write(INFO, "Sent data to client: %s\n", buffer);
}

int
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


int
accept_connection(int client_socket, int proxy_socket, struct sockaddr_in *client_address) 
{
    socklen_t client_len = sizeof(client_address);
    client_socket = accept(proxy_socket, (struct sockaddr *)&client_address, &client_len);
    if (client_socket == -1)
    {
        log_write(ERROR, "Client connection accept error");
        perror("Connection accept error");
    }
    return client_socket;
}

void 
run_proxy()
{
    int proxy_socket, client_socket, postgres_socket;
    struct sockaddr_in server_address, client_address;

    log_open();  

    proxy_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socket == -1)
    {
        log_write(ERROR, "Proxy socket creating error");
        exit(1);
    }

    postgres_socket = connect_postgres_server();


    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htons(INADDR_ANY);
    server_address.sin_port = htons(DEFAULT_POSTGRES_PORT);

    if (bind(proxy_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
    {
        log_write(ERROR, "Proxy's socket binding error");
        exit(1);
    }

    if (listen(proxy_socket, 10) == -1)
    {
        log_write(ERROR, "Proxy's socket listening error");
        exit(1);
    }

    log_write(INFO, "The proxy server is running. Waiting for connections...");

    /*
     * BAYARTO :: Я временно убрала твоё мультиплексирование, давай сначала разобьем
     *            на разные процессы (см ниже), потом уже со спокойной душой (что все коннекты рабочие)
     *            сделаем мультиплексирование (достанем твою наработку из коммита) 
     */

    /* it is needed to make infinte loop in forked process for waiting and accepting new clients */
    client_socket = accept_connection(client_socket, proxy_socket, &client_address);

    /* it is needed to make this infinite loop in another forked loop */
    for (;;)
    {   
        /* ADD :: before handling data check if the connection is alive */

        handle_client_data(postgres_socket, client_socket);
        handle_postgres_data(postgres_socket, client_socket);
        /* FIX :: rarely connection is lost 
         * idk why it is happening
         */
    }

    close(client_socket);
    close(postgres_socket);
    close(proxy_socket);

    log_write(INFO, "All sockets were closed. Proxy server is shutting down...");

    log_close();
}

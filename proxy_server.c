#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "proxy.h"

#define BUFFER_SIZE 4096

void *handle_client(void *arg) {
    int client_socket = *(int *) arg;
    char buffer[BUFFER_SIZE];
    int server_socket;

    // Creating server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creating error");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Preparing server addres
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");  // Server IP
    server_address.sin_port = htons(8080);  // Server Port

    // Connecting with server
    if (connect(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("Server connecting error");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Recieving data from client
    int bytes_received;
    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        // Sending client data to server
        if (send(server_socket, buffer, bytes_received, 0) == -1) {
            perror("Client data sending error");
            close(client_socket);
            pthread_exit(NULL);
        }

        // Recieving response from server
        int bytes_sent;
        while ((bytes_sent = recv(server_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            // Sending server response to client
            if (send(client_socket, buffer, bytes_sent, 0) == -1) {
                perror("Server response sending error");
                close(client_socket);
                pthread_exit(NULL);
            }
        }
    }
    close(server_socket);
    close(client_socket);

    pthread_exit(NULL);
}

void run_proxy() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    int client_address_size = sizeof(client_address);
    pthread_t thread;

    // Proxy socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Proxy socket creating error");
        exit(1);
    }

    // Preparing socket address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(8888);  // Proxy port

    // Binding socket to proxy address
    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("Socket binding error");
        exit(1);
    }

    // Connection listening
    if (listen(server_socket, 10) == -1) {
        perror("Connection listening error");
        exit(1);
    }

    printf("The proxy server is running. Waiting for connections...\n");

    // Accepting connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *) &client_address, (socklen_t *) &client_address_size);
        if (client_socket == -1) {
            perror("Connection accepting error");
            exit(1);
        }

        printf("Connection accepted. Creating thread to handle client...\n");

        if (pthread_create(&thread, NULL, handle_client, &client_socket) != 0) {
            perror("Thread creating error");
            close(client_socket);
            continue;
        }

        // Detach thread for releasing resources
        pthread_detach(thread);
    }

    close(server_socket);
}
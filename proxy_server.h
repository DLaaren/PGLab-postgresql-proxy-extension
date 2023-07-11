#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

extern void handle_client(int server_socket, int client_socket);
extern void run_proxy();

#endif /* PROXY_SERVER_H */
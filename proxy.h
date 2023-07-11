#ifndef PROXY_H
#define PROXY_H

extern void handle_client(int server_socket, int client_socket);
extern void run_proxy();
extern void find_postgres_server_port();
extern int connect_postgres_server();


#endif /* PROXY_H */
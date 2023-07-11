#ifndef PROXY_H
#define PROXY_H

extern void handle_client(int server_socket, int client_socket);
extern void run_proxy();
extern int connect_to_postgres_server();
extern void send_data_to_postgres_server();

#endif /* PROXY_H */
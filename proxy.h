#ifndef PROXY_H
#define PROXY_H

/*
 * Start proxy
 */
void run_proxy();

/*
 * Get connection to postgres server from proxy
 */
int connect_postgres_server();

/*
 *  Send data to postgres server
 */
void send_data_postgres_server();

/*
 * 
 */
int accept_connection(int client_socket, int server_socket, struct sockaddr_in *client_address); 

/*
 *  Handle data gotten from a client
 */
void handle_client_data(int server_socket, int postgres_socket, int client_socket);


#endif /* PROXY_H */
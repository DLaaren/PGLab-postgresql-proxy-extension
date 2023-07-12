#ifndef PROXY_H
#define PROXY_H

/*
 * Start proxy
 */
extern void run_proxy();

/*
 * Get connection to postgres server from proxy
 */
extern int connect_postgres_server();

/*
 *  Send data to postgres server
 */
extern void send_data_postgres_server();

/*
 *  Process data gotten from a client
 */
extern void process_client_data(int server_socket, int client_socket);


#endif /* PROXY_H */
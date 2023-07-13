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
int accept_connection(int proxy_socket); 

#endif /* PROXY_H */
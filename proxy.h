/* contrib/proxy/proxy.h */

#ifndef PROXY_H
#define PROXY_H

#define BUFFER_SIZE 1024

typedef struct Channel {
    struct pollfd *front_fd;            /* client */
    struct pollfd *back_fd;             /* postgres */
    char front_to_back[BUFFER_SIZE];
    char back_to_front[BUFFER_SIZE];
    int bytes_received_from_front;
    int bytes_received_from_back;
} Channel;

static int max_nodes;
static char **arr_listening_socket_addrs;
static int *arr_listening_socket_ports;
static char **arr_node_addrs;

/*
 * Runs proxy
 */
extern void
run_proxy();

extern void
shutdown_proxy();

#endif /* PROXY_H */
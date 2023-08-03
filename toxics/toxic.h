#ifndef TOXIC_H
#define TOXIC_H

#include "../proxy.h"
// toxic_name_*port* -> Toxic *latency (example)
typedef struct {
    char *name; /* type + stream */
    char *type;
    char *stream; /* upstream (client -> server) or downstream (server -> client) */
    double toxicity;
    int running;
    void (*pipe)(int port); /* main toxic function */
} Toxic;

extern void init_toxic_registry(void *toxic_registry);
extern void register_toxic(void *toxic_registry, char *name, Toxic *toxic);
extern Datum run(int *channel_port, text *toxic_name);

// TODO добавить List *channels в shmem с помощью InitShmem, чтобы можно было менять состояние канала из любого места
#endif /* TOXIC_H */
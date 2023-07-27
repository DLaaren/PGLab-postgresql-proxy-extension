#ifndef TOXIC_H
#define TOXIC_H

#include "../proxy.h"

typedef struct {
    char *name; /* type + stream */
    char *type;
    char *stream; /* upstream (client -> server) or downstream (server -> client) */
    double toxicity;
    int running;
    void (*pipe)(Channel *channel); /* main toxic function */
} Toxic;

extern void init_toxic_registry(void *toxic_registry);
extern void register_toxic(void *toxic_registry, char *name, Toxic *toxic);
extern void run(Channel *channel, Toxic *toxic);

#endif /* TOXIC_H */
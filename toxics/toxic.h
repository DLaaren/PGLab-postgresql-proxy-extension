#ifndef TOXIC_H
#define TOXIC_H

#include <stdio.h>

#include "../proxy.h"

typedef struct {
    char *name; /* type + stream */
    char *type;
    char *stream; /* upstream (client -> server) or downstream (server -> client) */
    double toxicity;
    void (*pipe)(Channel *channel); /* main toxic function */
} Toxic;

extern void register_toxic(Toxic toxic);

#endif /* TOXIC_H */
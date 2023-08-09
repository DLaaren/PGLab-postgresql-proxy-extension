#ifndef TOXIC_H
#define TOXIC_H

#include "postgres.h"
#include "fmgr.h"
#include "nodes/pg_list.h"
#include "../proxy.h"

// toxic_name_*port* -> Toxic *latency (example)
typedef struct {
    char *name; /* type + stream */
    char *type;
    double toxicity;
    int running;
    void (*pipe)(Channel *); /* main toxic function */
} Toxic;

extern List *init_toxic_registry();
extern void register_toxic(void *toxic_registry, char *name, Toxic *toxic);
extern Datum run(PG_FUNCTION_ARGS);

// TODO добавить List *channels в shmem с помощью InitShmem, чтобы можно было менять состояние канала из любого места
#endif /* TOXIC_H */
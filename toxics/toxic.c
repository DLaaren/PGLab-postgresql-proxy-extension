
#include <stdio.h>
#include <stdlib.h>

#include "postgres.h"

#include "toxic.h"

PG_FUNCTION_INFO_V1(run);

/* generate a random floating point number from min to max */
static double randfrom(double min, double max) 
{
    double range = (max - min); 
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

/*
 * Toxic registry initialization ()
 */
/* FIXME change (void *) type to map type */
void init_toxic_registry(void *toxic_registry)
{
    /* TODO create map which will contain toxics */
}

/*
 * Register toxic in toxic registry 
 */
void register_toxic(void *toxic_registry, char *name, Toxic *toxic)
{
    /*TODO: need to create map (from another lib or on my own)*/
}

/* TODO разобраться с тем, как доставать параметры функции с помощью PG_GETARG_* */
Datum run(PG_FUNCTION_ARGS)
{
    Channel *channel = (Channel *) PG_GETARG_POINTER(0);
    Toxic *toxic = (Toxic *) PG_GETARG_POINTER(1);
    toxic->running = 1;
    if (randfrom(0.0, 1.0) < toxic->toxicity) {
        toxic->pipe(channel);
    }
    else {
        /* FIXME run some toxic which do nothing (NoopToxic) */
        return;
    }
}

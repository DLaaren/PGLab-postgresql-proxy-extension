
#include <stdio.h>
#include <stdlib.h>

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
 * Toxic registry initialization
 */
List *init_toxic_registry()
{
    /* TODO create List which will contain toxics
     * Создать список
     * Занести его в shmem
     * Зарегать токсики в этом списке с помощью register_toxic
     */
    List *toxic_registry = NIL;

    // List *toxic_registry = toxic_registry
    return toxic_registry;
}

/*
 * Register toxic in toxic registry 
 */
void register_toxic(void *toxic_registry, char *name, Toxic *toxic)
{
    /*TODO: need to create map (from another lib or on my own)*/
}

/*  */
Datum run(PG_FUNCTION_ARGS)
{
    Channel *channel_port = (Channel *) PG_GETARG_INT32(0);
    text *toxic_name = PG_GETARG_TEXT_PP(1);

    /* TODO по имени токсика достать указатель на этот токсик */

    List *toxic_registry = init_toxic_registry();
    Toxic toxic = list.find(toxic_name);
    toxic.pipe(channel_port);
    
    // Toxic *toxic = map.get(toxic_name);

    toxic->running = 1;
    /* TODO из разделяемой памяти достать значение toxicity */

    // toxic->toxicity = shmem.toxicity;
    
    if (randfrom(0.0, 1.0) <= toxic->toxicity) {
        toxic->pipe(channel);
    }
    else {
        /* FIXME run some toxic which do nothing (NoopToxic) */
        return;
    }
}

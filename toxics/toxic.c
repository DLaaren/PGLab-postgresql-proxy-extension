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
    if (PG_ARGISNULL(0)) {
        elog(ERROR, "Cannot get name of toxic and channel port.\nrun(<toxic_name>, <port>)");
        PG_RETURN_VOID();
    }

    int channel_port = PG_GETARG_INT32(0);
    Channel *channel = find_channel(channel_port);
    if (channel_port == NULL) {
        elog(ERROR, "Cannot find channel port %d", channel_port);
        PG_RETURN_VOID();
    }

    text *toxic_name = PG_GETARG_TEXT_PP(1);
    List *toxic_registry = init_toxic_registry();

    /* TODO по имени токсика достать указатель на этот токсик */
    Toxic *toxic = find_toxic(*toxic_name, toxic_registry);
    if (toxic == NULL) {
        elog(ERROR, "Cannot find toxic %s", toxic_name);
        PG_RETURN_VOID();
    }
    //toxic->running = 1;
    /* TODO из разделяемой памяти достать значение toxicity */

    // toxic->toxicity = shmem.toxicity;
    
    if (randfrom(0.0, 1.0) <= toxic->toxicity) {
        toxic->pipe(channel_port);
        elog(INFO, "%s is working.", toxic_name);
    }
    else {
        elog(INFO, "%s is not working.", toxic_name);
    }
    PG_RETURN_VOID();;
}

Toxic *find_toxic(text toxic_name, List *toxic_registry) {
    ListCell *cell;
    foreach(cell, toxic_registry) {
        Toxic *current_toxic = lfirst(cell);
        if (strcmp(current_toxic->name, toxic_name.vl_dat) == 0) {
            return current_toxic;
        }
    }
    return NULL;
}


Channel *find_channel(int port) {
    List *proxy_channels = init_proxy_channels(); 
    ListCell *cell;
    foreach(cell, proxy_channels) {
        Channel *channel = lfirst(cell);
        if (channel->port == port) {
            return channel;
        }
    }
    return NULL;
}

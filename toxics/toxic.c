#include "toxic.h"

/*
 * Register toxic in map (toxic name: toxic) TODO: need to map (from another lib or my own)
 */
void register_toxic(char *name, Toxic toxic) {
    // register toxic in some map
}

void run(Channel* channel, Toxic toxic) {
    toxic.pipe(channel);
}

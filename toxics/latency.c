#include "toxic.h"
#include "latency.h"

Toxic init_latency(Channel *channel) {
    // TODO: с помощью register_toxic
    Toxic toxic;
    toxic.name = "latency";
    toxic.pipe = do_latency;
    return toxic;
}

// pipe
void *do_latency(Channel *channel) {
    channel->bytes_received_from_front = 0;
}
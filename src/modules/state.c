#include "state.h"
#include "bakery.h"
#include "pending_tx.h"

void init_estado(StoreState *s, double limite) {
    s->own_revenue = 0.0;
    s->chain_revenue = 0.0;
    s->max_revenue_limit = limite;
    s->status = STORE_STATUS_OPEN;
    s->current_ticket = 0;
    s->should_exit = false;
    bakery_lock_init(&s->lock);
    cola_init(&s->pending_tx);
}

void init_peers(PeerTable *t) {
    t->total_peers = 0;
    t->connection_counter = 1;
    bakery_lock_init(&t->lock);

    for (int i = 0; i < MAX_PEERS; i++) {
        t->peers[i].socket_fd = -1;
        t->peers[i].address = (struct sockaddr_in){0};
        t->peers[i].is_active = false;
        t->peers[i].peer_id = -1;
        t->peers[i].peer_type = STORE_TYPE_INDEPENDENT;
        t->peers[i].connection_order = 0;
    }
}
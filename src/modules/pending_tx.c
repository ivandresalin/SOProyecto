#include "pending_tx.h"

void cola_init(PendingTxQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool cola_push(PendingTxQueue *q, const MessagePacket *pkt, uint32_t conn_order, uint32_t ticket) {
    if (q->count >= MAX_PENDING_TX) return false;

    PendingTransaction *n = &q->queue[q->tail];
    n->packet = *pkt;
    n->connection_order = conn_order;
    n->ticket_number = ticket;

    q->tail = (q->tail + 1) % MAX_PENDING_TX;
    q->count++;
    return true;
}

bool cola_pop(PendingTxQueue *q, PendingTransaction *out) {
    if (q->count == 0) return false;

    *out = q->queue[q->head];
    q->head = (q->head + 1) % MAX_PENDING_TX;
    q->count--;
    return true;
}

// Insercion sobre el buffer circular: primero orden de conexion, luego ticket.
void cola_ordenar(PendingTxQueue *q) {
    for (int i = 1; i < q->count; i++) {
        PendingTransaction key = q->queue[(q->head + i) % MAX_PENDING_TX];
        int j = i - 1;

        while (j >= 0) {
            PendingTransaction *prev = &q->queue[(q->head + j) % MAX_PENDING_TX];
            bool antes = prev->connection_order > key.connection_order ||
                         (prev->connection_order == key.connection_order &&
                          prev->ticket_number > key.ticket_number);
            if (!antes) break;

            q->queue[(q->head + j + 1) % MAX_PENDING_TX] = *prev;
            j--;
        }
        q->queue[(q->head + j + 1) % MAX_PENDING_TX] = key;
    }
}
#ifndef PENDING_TX_H
#define PENDING_TX_H

#include "store_p2p.h"

void cola_init(PendingTxQueue *q);
bool cola_push(PendingTxQueue *q, const MessagePacket *pkt, uint32_t conn_order, uint32_t ticket);
bool cola_pop(PendingTxQueue *q, PendingTransaction *out);
void cola_ordenar(PendingTxQueue *q);

#endif
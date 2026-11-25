#ifndef THREADS_H
#define THREADS_H

#include "store_p2p.h"

void *hilo_peer(void *arg);
void *hilo_escucha(void *arg);

#endif
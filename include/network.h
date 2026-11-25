#ifndef NETWORK_H
#define NETWORK_H

#include "store_p2p.h"

ssize_t enviar_todo(int fd, const void *buf, size_t len);
ssize_t recibir_todo(int fd, void *buf, size_t len);

#endif
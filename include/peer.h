#ifndef PEER_H
#define PEER_H

#include "store_p2p.h"

uint32_t registrar_peer(PeerTable *t, int fd, struct sockaddr_in addr, int peer_id, StoreType tipo);
void quitar_peer(PeerTable *t, int fd);
void difundir(PeerTable *t, MessagePacket *pkt, int saltar_fd);
void conectar_remoto(AppContext *app, const char *ip, int puerto, int peer_id);
bool peer_conocido(PeerTable *t, int peer_id);
void fijar_puerto_peer(PeerTable *t, int fd, int puerto);
void lista_peers(PeerTable *t, char *buffer, size_t tam);
void cerrar_peers(PeerTable *t);

#endif
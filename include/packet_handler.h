#ifndef PACKET_HANDLER_H
#define PACKET_HANDLER_H

#include "store_p2p.h"

void procesar_pendientes(AppContext *app);
void manejar_paquete(AppContext *app, MessagePacket *pkt, int fd_cliente, struct sockaddr_in dir_cliente);
void apagon(AppContext *app);
void cerrar_app(AppContext *app);
bool hay_apagon(AppContext *app);
int turno_estado(AppContext *app);

#endif
#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "store_p2p.h"

bool abrir_log(AppContext *app);
void escribir_log(int fd, int store_id, double monto, const char *tipo, double total_cadena);

#endif
#include "network.h"

ssize_t enviar_todo(int fd, const void *buf, size_t len) {
    const char *p = buf;
    size_t enviados = 0;

    while (enviados < len) {
        ssize_t n = send(fd, p + enviados, len - enviados, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        enviados += (size_t)n;
    }
    return (ssize_t)enviados;
}

ssize_t recibir_todo(int fd, void *buf, size_t len) {
    char *p = buf;
    size_t recibidos = 0;

    while (recibidos < len) {
        ssize_t n = recv(fd, p + recibidos, len - recibidos, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;  // el par cerro la conexion
        recibidos += (size_t)n;
    }
    return (ssize_t)recibidos;
}
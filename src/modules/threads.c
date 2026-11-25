#include "threads.h"
#include "packet_handler.h"
#include "network.h"
#include "peer.h"

void *hilo_peer(void *arg) {
    WorkerContext *ctx = arg;
    AppContext *app = ctx->app_ctx;
    int fd = ctx->client_fd;
    struct sockaddr_in dir = ctx->client_addr;
    free(ctx);

    MessagePacket pkt;
    while (recibir_todo(fd, &pkt, sizeof pkt) == (ssize_t)sizeof pkt) {
        manejar_paquete(app, &pkt, fd, dir);
        if (hay_apagon(app))
            break;
    }

    quitar_peer(&app->peer_table, fd);
    close(fd);
    return NULL;
}

void *hilo_escucha(void *arg) {
    ListenerContext *ctx = arg;
    AppContext *app = ctx->app_ctx;
    int fd_escucha = ctx->listen_fd;
    free(ctx);

    while (!hay_apagon(app)) {
        struct pollfd pfd = {fd_escucha, POLLIN, 0};
        int r = poll(&pfd, 1, TIMEOUT_POLL_MS);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            continue;
        }
        if (r == 0 || !(pfd.revents & POLLIN))
            continue;

        struct sockaddr_in dir;
        socklen_t tam = sizeof(dir);
        int fd_cliente = accept(fd_escucha, (struct sockaddr *)&dir, &tam);
        if (fd_cliente < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        int idx = atomic_fetch_add(&app->worker_count, 1);
        if (idx >= MAX_WORKERS) {
            close(fd_cliente);
            continue;
        }

        WorkerContext *nuevo = malloc(sizeof *nuevo);
        if (!nuevo) {
            close(fd_cliente);
            continue;
        }
        nuevo->client_fd = fd_cliente;
        nuevo->client_addr = dir;
        nuevo->app_ctx = app;

        pthread_t hilo;
        if (pthread_create(&hilo, NULL, hilo_peer, nuevo) != 0) {
            perror("pthread_create");
            close(fd_cliente);
            free(nuevo);
            continue;
        }
        app->workers[idx] = hilo;
    }
    return NULL;
}
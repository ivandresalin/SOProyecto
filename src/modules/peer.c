#include "peer.h"
#include "bakery.h"
#include "network.h"
#include "threads.h"

static _Thread_local int id_local = -1;

static int mi_id(PeerTable *t) {
    if (id_local < 0)
        id_local = bakery_lock_register_thread(&t->lock);
    return id_local;
}

uint32_t registrar_peer(PeerTable *t, int fd, struct sockaddr_in addr, int peer_id, StoreType tipo) {
    int id = mi_id(t);
    if (id < 0) return 0;
    bakery_lock_acquire(&t->lock, id);

    int libre = -1;
    for (int i = 0; i < MAX_PEERS; i++) {
        if (t->peers[i].is_active && t->peers[i].socket_fd == fd) {
            t->peers[i].peer_id = peer_id;
            t->peers[i].peer_type = tipo;
            uint32_t orden = t->peers[i].connection_order;
            bakery_lock_release(&t->lock, id);
            return orden;
        }
        if (!t->peers[i].is_active && libre < 0)
            libre = i;
    }

    if (libre < 0) {
        bakery_lock_release(&t->lock, id);
        return 0;
    }

    PeerNode *n = &t->peers[libre];
    n->socket_fd = fd;
    n->address = addr;
    n->peer_id = peer_id;
    n->peer_type = tipo;
    n->is_active = true;
    n->connection_order = t->connection_counter++;
    t->total_peers++;

    uint32_t orden = n->connection_order;
    bakery_lock_release(&t->lock, id);
    return orden;
}

void quitar_peer(PeerTable *t, int fd) {
    int id = mi_id(t);
    if (id < 0) return;
    bakery_lock_acquire(&t->lock, id);

    for (int i = 0; i < MAX_PEERS; i++) {
        if (t->peers[i].is_active && t->peers[i].socket_fd == fd) {
            t->peers[i].is_active = false;
            t->peers[i].socket_fd = -1;
            t->peers[i].peer_id = -1;
            t->total_peers--;
            break;
        }
    }
    bakery_lock_release(&t->lock, id);
}

void difundir(PeerTable *t, MessagePacket *pkt, int saltar_fd) {
    int id = mi_id(t);
    if (id < 0) return;

    int fds[MAX_PEERS];
    int n = 0;

    bakery_lock_acquire(&t->lock, id);
    for (int i = 0; i < MAX_PEERS; i++)
        if (t->peers[i].is_active && t->peers[i].socket_fd != saltar_fd)
            fds[n++] = t->peers[i].socket_fd;
    bakery_lock_release(&t->lock, id);

    // enviar fuera del lock evita trabar a los demas peers si un socket se bloquea
    for (int i = 0; i < n; i++) {
        if (enviar_todo(fds[i], pkt, sizeof *pkt) < 0)
            perror("difundir");
    }
}

void conectar_remoto(AppContext *app, const char *ip, int puerto, int peer_id) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in dir = {0};
    dir.sin_family = AF_INET;
    dir.sin_port = htons(puerto);
    if (inet_pton(AF_INET, ip, &dir.sin_addr) != 1) {
        perror("inet_pton");
        close(fd);
        return;
    }

    if (connect(fd, (struct sockaddr *)&dir, sizeof dir) < 0) {
        perror("connect");
        close(fd);
        return;
    }

    int idx = atomic_fetch_add(&app->worker_count, 1);
    if (idx >= MAX_WORKERS) {
        close(fd);
        return;
    }

    registrar_peer(&app->peer_table, fd, dir, peer_id, STORE_TYPE_INDEPENDENT);

    WorkerContext *ctx = malloc(sizeof *ctx);
    if (!ctx) {
        quitar_peer(&app->peer_table, fd);
        close(fd);
        return;
    }
    ctx->client_fd = fd;
    ctx->client_addr = dir;
    ctx->app_ctx = app;

    pthread_t hilo;
    if (pthread_create(&hilo, NULL, hilo_peer, ctx) != 0) {
        perror("pthread_create");
        quitar_peer(&app->peer_table, fd);
        close(fd);
        free(ctx);
        return;
    }
    app->workers[idx] = hilo;

    MessagePacket hs = nuevo_paquete(MSG_HANDSHAKE, app->config.store_id, app->config.type);
    snprintf(hs.payload, sizeof hs.payload, "%d", app->config.listen_port);
    if (enviar_todo(fd, &hs, sizeof hs) < 0)
        perror("enviar handshake");
}

bool peer_conocido(PeerTable *t, int peer_id) {
    int id = mi_id(t);
    if (id < 0) return false;
    bakery_lock_acquire(&t->lock, id);

    bool existe = false;
    for (int i = 0; i < MAX_PEERS; i++) {
        if (t->peers[i].is_active && t->peers[i].peer_id == peer_id) {
            existe = true;
            break;
        }
    }
    bakery_lock_release(&t->lock, id);
    return existe;
}

void lista_peers(PeerTable *t, char *buffer, size_t tam) {
    if (tam == 0) return;

    int id = mi_id(t);
    if (id < 0) {
        buffer[0] = '\0';
        return;
    }
    bakery_lock_acquire(&t->lock, id);

    size_t usados = 0;
    buffer[0] = '\0';
    for (int i = 0; i < MAX_PEERS; i++) {
        if (!t->peers[i].is_active) continue;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &t->peers[i].address.sin_addr, ip, sizeof ip);

        int n = snprintf(buffer + usados, tam - usados, "%d:%u:%s:%d;",
                         t->peers[i].peer_id, t->peers[i].connection_order,
                         ip, ntohs(t->peers[i].address.sin_port));
        if (n < 0 || (size_t)n >= tam - usados)
            break;
        usados += (size_t)n;
    }
    bakery_lock_release(&t->lock, id);
}

void fijar_puerto_peer(PeerTable *t, int fd, int puerto) {
    int id = mi_id(t);
    if (id < 0) return;

    bakery_lock_acquire(&t->lock, id);
    for (int i = 0; i < MAX_PEERS; i++) {
        if (t->peers[i].is_active && t->peers[i].socket_fd == fd) {
            t->peers[i].address.sin_port = htons((uint16_t)puerto);
            break;
        }
    }
    bakery_lock_release(&t->lock, id);
}

void cerrar_peers(PeerTable *t) {
    int id = mi_id(t);
    bool con_lock = id >= 0;
    if (con_lock)
        bakery_lock_acquire(&t->lock, id);

    for (int i = 0; i < MAX_PEERS; i++) {
        if (t->peers[i].is_active && t->peers[i].socket_fd >= 0) {
            // shutdown despierta al worker; el close lo hace el propio hilo.
            shutdown(t->peers[i].socket_fd, SHUT_RDWR);
            t->peers[i].is_active = false;
            t->peers[i].socket_fd = -1;
        }
    }

    if (con_lock)
        bakery_lock_release(&t->lock, id);
}
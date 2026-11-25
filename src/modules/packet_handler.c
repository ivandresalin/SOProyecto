#include "packet_handler.h"
#include "bakery.h"
#include "pending_tx.h"
#include "persistence.h"
#include "peer.h"
#include "network.h"

static _Thread_local int id_estado = -1;

int turno_estado(AppContext *app) {
    if (id_estado < 0)
        id_estado = bakery_lock_register_thread(&app->state.lock);
    return id_estado;
}

bool hay_apagon(AppContext *app) {
    int id = turno_estado(app);
    if (id < 0) return false;

    bakery_lock_acquire(&app->state.lock, id);
    bool apagado = app->state.should_exit;
    bakery_lock_release(&app->state.lock, id);
    return apagado;
}

void procesar_pendientes(AppContext *app) {
    int id = turno_estado(app);
    if (id < 0) return;
    bakery_lock_acquire(&app->state.lock, id);

    cola_ordenar(&app->state.pending_tx);

    PendingTransaction tx;
    while (cola_pop(&app->state.pending_tx, &tx)) {
        app->state.chain_revenue += tx.packet.amount;
        double total = app->state.chain_revenue;

        escribir_log(app->log_fd, tx.packet.sender_id, tx.packet.amount,
                     "VENTA_REMOTA_CADENA", total);

        printf("\n[Sincro P2P] Venta de Tienda %d: $%.2f (ConnOrder #%u, Ticket #%u)\n> ",
               tx.packet.sender_id, tx.packet.amount, tx.connection_order, tx.ticket_number);
        printf("[Ingresos cadena]: $%.2f / Limite: $%.2f\n> ", total, app->state.max_revenue_limit);
        fflush(stdout);

        if (total >= app->state.max_revenue_limit && app->state.status == STORE_STATUS_OPEN) {
            app->state.status = STORE_STATUS_CLOSED;
            app->state.should_exit = true;
        }
    }
    bakery_lock_release(&app->state.lock, id);
}

void manejar_paquete(AppContext *app, MessagePacket *pkt, int fd_cliente, struct sockaddr_in dir_cliente) {
    switch (pkt->type) {
    case MSG_HANDSHAKE: {
        uint32_t orden = registrar_peer(&app->peer_table, fd_cliente, dir_cliente,
                                        pkt->sender_id, (StoreType)pkt->sender_type);
        printf("\n[P2P] Handshake de Tienda %d (orden #%u)\n> ", pkt->sender_id, orden);
        fflush(stdout);

        int puerto = atoi(pkt->payload);
        if (puerto > 0)
            fijar_puerto_peer(&app->peer_table, fd_cliente, puerto);

        if (app->config.type == STORE_TYPE_CHAIN) {
            MessagePacket lista = nuevo_paquete(MSG_PEER_LIST, app->config.store_id, app->config.type);
            char datos[256] = {0};
            lista_peers(&app->peer_table, datos, sizeof datos);
            snprintf(lista.payload, sizeof(lista.payload), "%s", datos);
            // avisar a todos: los demas aprenden del recien llegado y rediallean
            difundir(&app->peer_table, &lista, -1);
        }
        break;
    }

    case MSG_TRANSACTION: {
        if (app->config.type != STORE_TYPE_CHAIN || pkt->sender_type != STORE_TYPE_CHAIN)
            break;

        // registrar_peer actualiza el peer_id del enlace saliente y devuelve su orden.
        uint32_t orden = registrar_peer(&app->peer_table, fd_cliente, dir_cliente,
                                        pkt->sender_id, (StoreType)pkt->sender_type);

        int id = turno_estado(app);
        bool pusheado = false;
        if (id >= 0) {
            bakery_lock_acquire(&app->state.lock, id);
            pusheado = cola_push(&app->state.pending_tx, pkt, orden, pkt->ticket_number);
            bakery_lock_release(&app->state.lock, id);
        }

        if (!pusheado) {
            procesar_pendientes(app);
            if (id >= 0) {
                bakery_lock_acquire(&app->state.lock, id);
                pusheado = cola_push(&app->state.pending_tx, pkt, orden, pkt->ticket_number);
                bakery_lock_release(&app->state.lock, id);
            }
        }
        if (!pusheado)
            fprintf(stderr, "[aviso] cola llena, se descarta venta de tienda %d\n",
                    pkt->sender_id);

        procesar_pendientes(app);

        if (hay_apagon(app))
            apagon(app);
        break;
    }

    case MSG_PEER_LIST: {
        if (app->config.type != STORE_TYPE_CHAIN) break;

        char *token = strtok(pkt->payload, ";");
        while (token) {
            int peer_id = 0;
            uint32_t orden = 0;
            char ip[64];
            int puerto = 0;

            // solo el id menor inicia la conexion: evita enlaces duplicados
            if (sscanf(token, "%d:%u:%63[^:]:%d", &peer_id, &orden, ip, &puerto) == 4 &&
                peer_id != app->config.store_id && peer_id > app->config.store_id &&
                !peer_conocido(&app->peer_table, peer_id)) {
                printf("\n[Gossip] Conectando a peer %d (%s:%d)\n> ", peer_id, ip, puerto);
                fflush(stdout);
                conectar_remoto(app, ip, puerto, peer_id);
            }
            token = strtok(NULL, ";");
        }
        break;
    }

    case MSG_CONSENSUS_SYNC: {
        if (app->config.type != STORE_TYPE_CHAIN) break;

        double remoto = pkt->amount;
        uint32_t version = pkt->ticket_number;

        int id = turno_estado(app);
        if (id < 0) break;

        bool adoptado = false;
        bakery_lock_acquire(&app->state.lock, id);
        if (version > app->consensus_version ||
            (version == app->consensus_version && remoto > app->state.chain_revenue)) {
            app->state.chain_revenue = remoto;
            app->consensus_version = version;
            app->last_consensus_chain_revenue = remoto;
            adoptado = true;
        }
        bakery_lock_release(&app->state.lock, id);

        if (adoptado) {
            printf("\n[Consenso] chain_revenue = $%.2f (version %u)\n> ", remoto, version);
            fflush(stdout);
            difundir(&app->peer_table, pkt, fd_cliente);
        }
        break;
    }

    case MSG_GOV_QUERY: {
        MessagePacket resp = nuevo_paquete(MSG_GOV_RESPONSE, app->config.store_id, app->config.type);

        int id = turno_estado(app);
        if (id >= 0) {
            bakery_lock_acquire(&app->state.lock, id);
            resp.amount = app->state.own_revenue;
            snprintf(resp.payload, sizeof(resp.payload),
                     "ESTADO:%s | PROPIO:$%.2f | CADENA:$%.2f",
                     app->state.status == STORE_STATUS_OPEN ? "ABIERTO" : "CERRADO",
                     app->state.own_revenue, app->state.chain_revenue);
            bakery_lock_release(&app->state.lock, id);
        }
        enviar_todo(fd_cliente, &resp, sizeof resp);
        break;
    }

    case MSG_LIMIT_REACHED:
        printf("\n[Red] Tienda %d cerro por alcanzar su limite ($%.2f)\n> ",
               pkt->sender_id, pkt->amount);
        fflush(stdout);
        break;

    default:
        break;
    }
}

void apagon(AppContext *app) {
    bool cadena = app->config.type == STORE_TYPE_CHAIN;
    double total = 0.0;
    double total_cadena = 0.0;
    uint32_t version = 0;

    int id = turno_estado(app);
    if (id >= 0) {
        bakery_lock_acquire(&app->state.lock, id);
        total = cadena ? app->state.chain_revenue : app->state.own_revenue;
        total_cadena = app->state.chain_revenue;
        if (cadena)
            version = ++app->consensus_version;
        bakery_lock_release(&app->state.lock, id);
    } else {
        total = cadena ? app->state.chain_revenue : app->state.own_revenue;
        total_cadena = app->state.chain_revenue;
    }

    printf("[ALERTA GOBIERNO] Limite alcanzado ($%.2f >= $%.2f)\n",
           total, app->state.max_revenue_limit);
    printf("[BLACKOUT] Apagando el computador de la tienda...\n");
    fflush(stdout);

    if (cadena) {
        MessagePacket sync = nuevo_paquete(MSG_CONSENSUS_SYNC, app->config.store_id, app->config.type);
        sync.amount = total_cadena;
        sync.ticket_number = version;
        difundir(&app->peer_table, &sync, -1);
    }

    MessagePacket aviso = nuevo_paquete(MSG_LIMIT_REACHED, app->config.store_id, app->config.type);
    aviso.amount = total;
    difundir(&app->peer_table, &aviso, -1);

    struct timespec espera = {ESPERA_APAGON_MS / 1000, (ESPERA_APAGON_MS % 1000) * 1000000L};
    nanosleep(&espera, NULL);

    cerrar_app(app);
    exit(EXIT_SUCCESS);
}

void cerrar_app(AppContext *app) {
    if (atomic_exchange(&app->cerrado, true))
        return;

    cerrar_peers(&app->peer_table);

    int id = turno_estado(app);
    if (id >= 0) {
        bakery_lock_acquire(&app->state.lock, id);
        if (app->log_fd >= 0) {
            close(app->log_fd);
            app->log_fd = -1;
        }
        bakery_lock_release(&app->state.lock, id);
    } else if (app->log_fd >= 0) {
        close(app->log_fd);
        app->log_fd = -1;
    }
}
#include "store_p2p.h"
#include "args.h"
#include "state.h"
#include "persistence.h"
#include "peer.h"
#include "threads.h"
#include "packet_handler.h"
#include "network.h"
#include "bakery.h"

static void hacer_venta(AppContext *app, double monto) {
    int id = turno_estado(app);
    if (id < 0) return;

    bool apagar = false;
    uint32_t ticket = 0;

    bakery_lock_acquire(&app->state.lock, id);

    if (app->state.status == STORE_STATUS_CLOSED) {
        bakery_lock_release(&app->state.lock, id);
        return;
    }

    app->state.own_revenue += monto;
    if (app->config.type == STORE_TYPE_CHAIN)
        app->state.chain_revenue += monto;
    app->state.current_ticket++;
    ticket = app->state.current_ticket;

    double total = (app->config.type == STORE_TYPE_CHAIN) ?
                   app->state.chain_revenue : app->state.own_revenue;

    escribir_log(app->log_fd, app->config.store_id, monto,
                 "VENTA_PROPIA_ORIGEN", app->state.chain_revenue);

    printf("[Venta] $%.2f | Propio: $%.2f | Cadena: $%.2f (ticket #%u)\n",
           monto, app->state.own_revenue, app->state.chain_revenue, ticket);

    if (total >= app->state.max_revenue_limit) {
        app->state.status = STORE_STATUS_CLOSED;
        app->state.should_exit = true;
        apagar = true;
    }
    bakery_lock_release(&app->state.lock, id);

    MessagePacket tx = nuevo_paquete(MSG_TRANSACTION, app->config.store_id, app->config.type);
    tx.amount = monto;
    tx.ticket_number = ticket;
    difundir(&app->peer_table, &tx, -1);

    if (apagar)
        apagon(app);
}

static void mostrar_reporte(AppContext *app) {
    int id = turno_estado(app);
    if (id < 0) return;

    bakery_lock_acquire(&app->state.lock, id);

    printf("\n--- MONITOREO EMPRESA ELECTRICA ---\n");
    printf("Tienda          : %d\n", app->config.store_id);
    printf("Estado          : %s\n",
           app->state.status == STORE_STATUS_OPEN ? "ABIERTO" : "CERRADO");
    printf("Ventas propias  : $%.2f\n", app->state.own_revenue);
    if (app->config.type == STORE_TYPE_CHAIN)
        printf("Ingresos cadena : $%.2f\n", app->state.chain_revenue);
    printf("Limite          : $%.2f\n", app->state.max_revenue_limit);

    bakery_lock_release(&app->state.lock, id);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    AppContext app = {0};
    atomic_init(&app.worker_count, 0);
    atomic_init(&app.cerrado, false);

    int fd_escucha = -1;
    int rc = EXIT_SUCCESS;
    bool escucha_activa = false;

    if (!leer_args(argc, argv, &app.config))
        return EXIT_FAILURE;

    init_peers(&app.peer_table);
    init_estado(&app.state, app.config.max_revenue_limit);
    if (!abrir_log(&app)) {
        rc = EXIT_FAILURE;
        goto limpiar;
    }

    fd_escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_escucha < 0) {
        perror("socket");
        rc = EXIT_FAILURE;
        goto limpiar;
    }

    int opt = 1;
    setsockopt(fd_escucha, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in dir = {0};
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = INADDR_ANY;
    dir.sin_port = htons(app.config.listen_port);

    if (bind(fd_escucha, (struct sockaddr *)&dir, sizeof dir) < 0) {
        perror("bind");
        rc = EXIT_FAILURE;
        goto limpiar;
    }
    if (listen(fd_escucha, COLA_ESCUCHA) < 0) {
        perror("listen");
        rc = EXIT_FAILURE;
        goto limpiar;
    }

    printf("[BLACKOUT] Tienda %d iniciada\n", app.config.store_id);
    printf("Tipo: %s | Estado: ABIERTO | Puerto: %d\n",
           app.config.type == STORE_TYPE_CHAIN ? "Cadena" : "Independiente",
           app.config.listen_port);
    printf("Limite de energia (gobierno): $%.2f\n", app.config.max_revenue_limit);

    ListenerContext *lctx = malloc(sizeof *lctx);
    if (!lctx) {
        perror("malloc");
        rc = EXIT_FAILURE;
        goto limpiar;
    }
    lctx->app_ctx = &app;
    lctx->listen_fd = fd_escucha;

    pthread_t tid_escucha;
    if (pthread_create(&tid_escucha, NULL, hilo_escucha, lctx) != 0) {
        perror("pthread_create");
        free(lctx);
        rc = EXIT_FAILURE;
        goto limpiar;
    }
    escucha_activa = true;

    if (app.config.target_peer_ip[0] && app.config.target_peer_port > 0) {
        printf("[P2P] Conectando al vecino %s:%d\n",
               app.config.target_peer_ip, app.config.target_peer_port);
        conectar_remoto(&app, app.config.target_peer_ip, app.config.target_peer_port, -1);
    }

    char linea[128];
    for (;;) {
        printf("> ");
        fflush(stdout);

        if (!fgets(linea, sizeof linea, stdin)) {
            // Sin entrada (pipe cerrado): seguir vivo hasta que llegue el apagon.
            if (feof(stdin)) {
                while (!hay_apagon(&app))
                    sleep(1);
            }
            break;
        }

        linea[strcspn(linea, "\r\n")] = '\0';

        char *cmd = linea;
        while (*cmd == ' ') cmd++;

        if (strncmp(cmd, "vender ", 7) == 0) {
            double monto = atof(cmd + 7);
            if (monto > 0) hacer_venta(&app, monto);
        } else if (strcmp(cmd, "monitorear") == 0 || strcmp(cmd, "stats") == 0) {
            mostrar_reporte(&app);
        } else if (strcmp(cmd, "exit") == 0) {
            printf("Cerrando sockets y finalizando nodo...\n");
            break;
        } else {
            double monto = atof(cmd);
            if (monto > 0) hacer_venta(&app, monto);
        }
    }

    int id = turno_estado(&app);
    if (id >= 0) {
        bakery_lock_acquire(&app.state.lock, id);
        app.state.should_exit = true;
        bakery_lock_release(&app.state.lock, id);
    }

    if (escucha_activa)
        pthread_join(tid_escucha, NULL);

    cerrar_peers(&app.peer_table);

    int n = atomic_load(&app.worker_count);
    if (n > MAX_WORKERS) n = MAX_WORKERS;
    for (int i = 0; i < n; i++)
        if (app.workers[i])
            pthread_join(app.workers[i], NULL);

limpiar:
    if (fd_escucha >= 0)
        close(fd_escucha);
    cerrar_app(&app);
    return rc;
}
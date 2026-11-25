#include "args.h"

static bool entero(const char *txt, int min, int max, int *valor) {
    char *fin;
    errno = 0;
    long v = strtol(txt, &fin, 10);
    if (errno || fin == txt || v < min || v > max)
        return false;
    *valor = (int)v;
    return true;
}

bool leer_args(int argc, char *argv[], StoreConfig *config) {
    if (argc < 5) {
        fprintf(stderr,
                "Uso: %s <ID> <tipo: 0=Indep, 1=Cadena> <puerto> <limite> [ip_par] [puerto_par]\n",
                argv[0]);
        return false;
    }

    int valor;
    if (!entero(argv[1], 0, INT_MAX, &valor)) {
        fprintf(stderr, "ID de tienda invalido: %s\n", argv[1]);
        return false;
    }
    config->store_id = valor;

    if (!entero(argv[2], 0, 1, &valor)) {
        fprintf(stderr, "Tipo invalido (0=Independiente, 1=Cadena): %s\n", argv[2]);
        return false;
    }
    config->type = (StoreType)valor;

    if (!entero(argv[3], 1, 65535, &valor)) {
        fprintf(stderr, "Puerto invalido (1-65535): %s\n", argv[3]);
        return false;
    }
    config->listen_port = valor;

    char *fin;
    errno = 0;
    double limite = strtod(argv[4], &fin);
    if (errno || fin == argv[4] || limite <= 0.0) {
        fprintf(stderr, "Limite de ingresos invalido (> 0): %s\n", argv[4]);
        return false;
    }
    config->max_revenue_limit = limite;

    config->target_peer_ip[0] = '\0';
    config->target_peer_port = 0;

    if (argc >= 7) {
        snprintf(config->target_peer_ip, sizeof(config->target_peer_ip), "%s", argv[5]);
        if (!entero(argv[6], 1, 65535, &valor)) {
            fprintf(stderr, "Puerto del par invalido (1-65535): %s\n", argv[6]);
            return false;
        }
        config->target_peer_port = valor;
    }

    return true;
}
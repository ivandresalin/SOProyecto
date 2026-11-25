#include "persistence.h"

bool abrir_log(AppContext *app) {
    snprintf(app->config.log_filename, sizeof(app->config.log_filename),
             "tienda_%d_blackout.log", app->config.store_id);

    app->log_fd = open(app->config.log_filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (app->log_fd < 0) {
        perror("abrir log");
        return false;
    }
    return true;
}

void escribir_log(int fd, int store_id, double monto, const char *tipo, double total_cadena) {
    if (fd < 0) return;

    char sello[64];
    time_t ahora = time(NULL);
    struct tm desglose;
    localtime_r(&ahora, &desglose);
    strftime(sello, sizeof(sello), "%Y-%m-%d %H:%M:%S", &desglose);

    char linea[256];
    int len = snprintf(linea, sizeof(linea),
                       "[%s] NODO_%d | TIPO: %s | MONTO: $%.2f | TOTAL_CADENA: $%.2f\n",
                       sello, store_id, tipo, monto, total_cadena);
    if (len <= 0 || (size_t)len >= sizeof(linea)) return;

    if (write(fd, linea, (size_t)len) < 0)
        perror("write log");
    fsync(fd);  // el corte de energia no debe perder la venta
}
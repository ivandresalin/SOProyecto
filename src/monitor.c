#include "store_p2p.h"
#include "network.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <ip> <puerto>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in dir = {0};
    dir.sin_family = AF_INET;
    dir.sin_port = htons((uint16_t)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &dir.sin_addr) != 1) {
        perror("inet_pton");
        close(fd);
        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr *)&dir, sizeof dir) < 0) {
        perror("connect");
        close(fd);
        return EXIT_FAILURE;
    }

    MessagePacket q = nuevo_paquete(MSG_GOV_QUERY, -1, STORE_TYPE_INDEPENDENT);
    if (enviar_todo(fd, &q, sizeof q) < 0) {
        perror("enviar consulta");
        close(fd);
        return EXIT_FAILURE;
    }

    MessagePacket r;
    if (recibir_todo(fd, &r, sizeof r) != (ssize_t)sizeof r) {
        perror("recibir respuesta");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Tienda %d (%s)\n", r.sender_id,
           r.sender_type == STORE_TYPE_CHAIN ? "Cadena" : "Independiente");
    printf("%s\n", r.payload);

    close(fd);
    return EXIT_SUCCESS;
}
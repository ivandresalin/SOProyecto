#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char *argv[]) {
    int intervalo = (argc > 1) ? atoi(argv[1]) : 1;
    if (intervalo <= 0) intervalo = 1;

    srand(time(NULL));
    setvbuf(stdout, NULL, _IONBF, 0);  // salida inmediata al conectarse por pipe

    for (;;) {
        double monto = 10.0 + ((double)rand() / RAND_MAX) * 490.0;
        printf("%.2f\n", monto);
        sleep(intervalo);
    }
    return 0;
}
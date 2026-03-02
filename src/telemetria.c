/* Archivo: telemetria.c
 *
 * Hilo de telemetría: muestra el estado
 * de las válvulas para que el operador (y los usuario vía terminal) vean la ocupación en tiempo real.
 */
#include "../include/telemetria.h"
#include "../include/nucleo_flujo.h"
#include "../include/logger.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void* agente_telemetria(void* arg) {
    (void)arg;

    /* Bucle del menú */
    while (1) {
        int libres   = nucleo_valvulas_libres();
        int ocupadas = NUM_VALVULAS - libres;

        /* Barra visual de ocupación*/
        char barra[NUM_VALVULAS + 1];
        for (int i = 0; i < NUM_VALVULAS; i++) {
            barra[i] = (i < ocupadas) ? '#' : '-';
        }
        barra[NUM_VALVULAS] = '\0';

        log_evento(COL_BLUE,
                   "TELEMETRÍA| [%s] %d/%d válvulas libres",
                   barra, libres, NUM_VALVULAS);

        usleep(TASA_REFRESCO_UI);
        pthread_testcancel(); /* Punto seguro de cancelación */
    }

    return NULL;
}

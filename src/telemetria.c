/* Archivo: telemetria.c
 *
 * Hilo de telemetría: muestra el estado
 * de las válvulas para que el operador (y los usuario vía terminal) vean la ocupación en tiempo real.
 *
 * APAGADO LIMPIO:
 *   El bucle se controla con la bandera 'simulacion_activa'.
 *   Cuando main pone la bandera en false, el hilo termina su bucle
 *   por sí solo al evaluar la condición del while, sin necesidad
 *   de pthread_cancel (que puede causar deadlocks si el hilo
 *   es cancelado mientras posee un semáforo internamente).
 */
#include "../include/telemetria.h"
#include "../include/nucleo_flujo.h"
#include "../include/agentes.h"
#include "../include/logger.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void* agente_telemetria(void* arg) {
    (void)arg;

    /* Bucle controlado por bandera de apagado limpio (sin pthread_cancel) */
    while (simulacion_activa) {
        /* Barra visual con posicion real de cada valvula (# = ocupada, - = libre).
         * Se reutiliza nucleo_get_estado_valvulas_str() que ya refleja el estado
         * real de cada nodo individual, en lugar de empaquetar los # a la izquierda. */
        char barra[NUM_VALVULAS + 3];
        nucleo_get_estado_valvulas_str(barra);
        int libres = nucleo_valvulas_libres();

        log_evento(COL_BLUE,
                   "TELEMETRIA| %s %d/%d valvulas libres",
                   barra, libres, NUM_VALVULAS);

        usleep(TASA_REFRESCO_UI);
        /* Se eliminó pthread_testcancel(): ya no se usa pthread_cancel,
         * el hilo sale limpiamente al evaluar simulacion_activa == false */
    }

    return NULL;
}

/* Archivo: logger.c
 * Implementación del módulo de logging visual
 *
 * Convención de tiempo simulado:
 *   DURACION_SIMULACION segundos reales  ≡  12 horas (06:00-18:00)
 *   → 1 hora simulada = DURACION_SIMULACION / 12.0  segundos reales
 *   → tiempo_hora_sim = segundos_reales * (12.0 / DURACION_SIMULACION)
 */
#include "../include/logger.h"
#include "../include/eco_config.h"
#include "../include/nucleo_flujo.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>

/* Variables de estado para el log */
__thread const char* tls_estado_hilo = "INACTIVO";

/* Estado del auditor protegido por semaforo dedicado (sem_estado_auditor).
 * Se usa un buffer interno y funciones getter/setter para evitar race
 * conditions entre el hilo auditor (escritor) y log_evento (lector). */
static char buf_estado_auditor[64] = "DURMIENDO";
static sem_t sem_estado_auditor;

/* Semaforo binario que serializa todos los printf del sistema.
 * Reemplaza al pthread_mutex_t anterior para cumplir con el requisito
 * del enunciado: sincronizacion exclusivamente con semaforos.          */
static sem_t sem_log;

/* Momento de inicio de la simulación (capturado en logger_init) */
static struct timespec t_inicio;

/* ── Funciones públicas */

void logger_init(void) {
    sem_init(&sem_log, 0, 1);
    sem_init(&sem_estado_auditor, 0, 1);
    clock_gettime(CLOCK_MONOTONIC, &t_inicio);
}

void logger_close(void) {
    sem_destroy(&sem_log);
    sem_destroy(&sem_estado_auditor);
}

void logger_set_estado_auditor(const char *estado) {
    sem_wait(&sem_estado_auditor);
    snprintf(buf_estado_auditor, sizeof(buf_estado_auditor), "%s", estado);
    sem_post(&sem_estado_auditor);
}

double logger_segundos_transcurridos(void) {
    struct timespec ahora;
    clock_gettime(CLOCK_MONOTONIC, &ahora);
    double seg = (double)(ahora.tv_sec  - t_inicio.tv_sec)
               + (double)(ahora.tv_nsec - t_inicio.tv_nsec) * 1e-9;
    return seg;
}

/**
 * Calcula la hora del día simulada (06:00-18:00) a partir
 * de los segundos reales transcurridos.
 *
 * @param seg_reales  Segundos reales desde inicio.
 * @param hora_out    Horas (6-18).
 * @param min_out     Minutos (0-59).
 */
static void segundos_a_hora_simulada(double seg_reales,
                                     int *hora_out, int *min_out) {
    /* Proporción descorrida [0, 1] */
    double prop = seg_reales / (double)DURACION_SIMULACION;
    if (prop > 1.0) prop = 1.0;

    /* 12 horas de operación: 06:00 → 18:00 */
    double minutos_totales = prop * 12.0 * 60.0;
    int h = (int)(minutos_totales / 60.0);
    int m = (int)(minutos_totales) % 60;

    *hora_out = 6 + h;    /* Turno comienza a las 06:00 */
    *min_out  = m;
}

void log_evento(const char *color, const char *fmt, ...) {
    double seg = logger_segundos_transcurridos();
    int hora, minuto;
    segundos_a_hora_simulada(seg, &hora, &minuto);

    sem_wait(&sem_log);

    char valvulas_str[NUM_VALVULAS + 3];
    nucleo_get_estado_valvulas_str(valvulas_str);

    /* Snapshot thread-safe del estado del auditor */
    char estado_aud[64];
    sem_wait(&sem_estado_auditor);
    snprintf(estado_aud, sizeof(estado_aud), "%s", buf_estado_auditor);
    sem_post(&sem_estado_auditor);

    /* Prefijo estricto de concurrencia */
    printf("%s[%02d:%02d] [TID: %lu] [%-11s] [%-18s] %s ", color, hora, minuto,
           (unsigned long)pthread_self(),
           tls_estado_hilo, estado_aud, valvulas_str);

    /* Mensaje del llamador */
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("%s\n", COL_RESET);
    fflush(stdout);

    sem_post(&sem_log);
}

void logger_reiniciar_reloj(void) {
    sem_wait(&sem_log);
    clock_gettime(CLOCK_MONOTONIC, &t_inicio);
    sem_post(&sem_log);
}

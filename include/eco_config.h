/* Archivo: eco_config.h */
#ifndef ECO_CONFIG_H
#define ECO_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

// --- Parametros ---
#define NUM_VALVULAS 10
#define DURACION_SIMULACION 3      // Segundos reales (Representan 12 horas: 06:00-18:00)
#define MAX_SOLICITUDES_DIA 250
#define LITROS_CRITICOS 500.0
#define TASA_REFRESCO_UI 500000    // 500ms de refresco para telemetria
#define TAM_COLA_AUDITOR 50        // Buffer tamano Auditor

// --- Escalado temporal derivado automaticamente ---
// DURACION_BASE_SEG es la duracion "canonica" de referencia (24s = 12h simuladas).
// FACTOR_ESCALA permite que al cambiar DURACION_SIMULACION, todos los tiempos
// internos (consumo, auditor) y las estadisticas se ajusten coherentemente.
#define DURACION_BASE_SEG  24.0
#define FACTOR_ESCALA      (DURACION_BASE_SEG / (double)DURACION_SIMULACION)
#define USLEEP_CONSUMO     ((useconds_t)(150000 / FACTOR_ESCALA))
#define USLEEP_AUDITOR     ((useconds_t)(150000 / FACTOR_ESCALA))

#endif // ECO_CONFIG_H

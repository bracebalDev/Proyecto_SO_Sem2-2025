/* Archivo: include/eco_config.h */
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

// --- Parametros del Enunciado ---
#define NUM_VALVULAS 10
#define DURACION_SIMULACION 30     // Segundos reales (Representan 12 horas: 06:00-18:00)
#define MAX_SOLICITUDES_DIA 250
#define LITROS_CRITICOS 500.0
#define UMBRAL_PROB_RESERVA 50     // 50% probabilidad
#define TASA_REFRESCO_UI 150000    // 150ms

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

// --- Parametrización del Enunciado ---
#define NUM_VALVULAS 10
#define DURACION_SIMULACION 30     // Segundos reales (Representan 12 horas: 06:00-18:00)
#define MAX_SOLICITUDES_DIA 250
#define LITROS_CRITICOS 500.0
#define UMBRAL_PROB_RESERVA 50     // 50% probabilidad
#define TASA_REFRESCO_UI 150000    // 150ms

// --- Códigos ANSI para Visualización ---
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"      // Ocupado / Error
#define ANSI_GREEN   "\033[32m"      // Libre / Éxito
#define ANSI_YELLOW  "\033[33m"      // Lectura / Advertencia
#define ANSI_BLUE    "\033[34m"      // Títulos
#define ANSI_CYAN    "\033[36m"      // Información
#define ANSI_BOLD    "\033[1m"

// --- Estructuras de Datos (Modelos) ---

// Modelo de Válvula (Recurso Compartido)
typedef struct {
    int id_nodo;
    int usuario_actual;       // ID del hilo dueño (-1 si libre)
    bool es_critico;          // Flag de uso exclusivo (Escritura)
    int lectores_activos;     // Contador para lectores concurrentes
    
    // Herramientas de Sincronización
    sem_t sem_exclusion;      // Semáforo Binario (Controla acceso a DB/Escritura)
    sem_t sem_lectores;       // Semáforo Binario (Protege variable 'lectores_activos')
    pthread_mutex_t mtx_ui;   // Mutex rápido para coherencia visual instantánea
} NodoFlujo;

// Estadísticas Globales (Monitor)
typedef struct {
    double m3_total_procesados;
    int amonestaciones_digitales;
    int consumos_criticos;    // > 500L
    int consumos_estandar;    // <= 500L
    int eficiencia_asignaciones; // Total atendidos
    int tiempo_espera_acumulado; // Simulado
    pthread_mutex_t mtx_stats;
} MetricasEco;

// Contexto para pasar argumentos a hilos
typedef struct {
    int uid;
    int tipo_usuario; // 0: Residencial, 1: Industrial
} ContextoAgente;

// --- Variables Globales (Externas) ---
extern NodoFlujo g_nodos[NUM_VALVULAS];
extern MetricasEco g_metricas;
extern volatile bool g_sistema_activo;

#endif
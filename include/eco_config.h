#ifndef ECO_FLOW_H
#define ECO_FLOW_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

// --- Constantes del Sistema ---
#define NUM_VALVULAS 10
#define HORA_INICIO 6
#define HORA_FIN 18
#define DURACION_SIMULACION_SEG 30 // Duración total de la simulación en tiempo real
#define MAX_USUARIOS_DIA 250
#define LITROS_CRITICOS 500

// Códigos de colores ANSI para la consola
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// --- Estructuras de Datos ---

// Estado de una Válvula (Nodo de Flujo)
typedef struct {
    int id;
    int usuario_id_actual;    // -1 si está libre
    bool ocupado;             // true si hay una reserva activa (escritura)
    int lectores_activos;     // Cantidad de usuarios consultando presión
    sem_t sem_acceso_db;      // Semáforo para Exclusión Mutua (Escritura)
    sem_t sem_lectores;       // Semáforo para proteger el contador de lectores
    pthread_mutex_t mutex_estado; // Mutex simple para cambios rápidos de estado visual
} Valvula;

// Estadísticas Globales (Protegidas por Mutex)
typedef struct {
    double total_m3_procesados;
    int amonestaciones_digitales;
    int consumos_criticos;
    int consumos_estandar;
    int usuarios_atendidos;
    int intentos_fallidos_reserva;
    pthread_mutex_t mutex_stats;
} Estadisticas;

// Argumentos para los hilos de usuario
typedef struct {
    int id_usuario;
    int tipo_usuario; // 0: Residencial, 1: Industrial
} InfoUsuario;

// --- Variables Globales Externas ---
extern Valvula sistema_valvulas[NUM_VALVULAS];
extern Estadisticas stats_globales;
extern bool simulacion_activa;

// --- Prototipos de Funciones ---

// sincronizacion.c
void inicializar_sistema();
void destruir_sistema();
bool solicitar_reserva(int id_usuario, int id_valvula);
void liberar_reserva(int id_usuario, int id_valvula, double litros_consumidos);
void consultar_presion(int id_usuario, int id_valvula);
void cancelar_solicitud(int id_usuario, int id_valvula);
void registrar_amonestacion();

// comportamiento.c
void* hilo_usuario(void* arg);
void* hilo_auditor(void* arg);
void* hilo_interfaz(void* arg);

// Utilidad
double generar_litros_consumo(int tipo_usuario);
int obtener_valvula_aleatoria();

#endif
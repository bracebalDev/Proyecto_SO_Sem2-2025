#ifndef DEFINICIONES_H
#define DEFINICIONES_H

#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h> // para fork y sleep
#include <sys/ipc.h> //para IPC
#include <sys/shm.h> //memoria compartida
#include <sys/sem.h> //semaforos
#include <sys/types.h> 
#include <sys/wait.h> 
#include <sys/resource.h> 
#include <time.h>
#include <stdbool.h>

// --- Configuración del Sistema Eco-Flow ---
#define NUM_NODOS 10           // Cantidad de válvulas disponibles
#define TOTAL_SOLICITUDES 250   // Total de solicitudes a realizar (propuestas por el caso base)
#define HORAS_SIMULACION 12    // De 06:00 a 18:00 (12 horas)
#define LIMITE_CRITICO 500     // Litros para validación del Auditor

// --- Probabilidades ---
#define PROB_RESERVA 50        // 50% de probabilidad de reservar

// --- Estructura de la Memoria Compartida ---
typedef struct {
    // Matriz de flujo: guarda el ID del proceso que reservó (0 si está libre)
    int matriz_reservas[HORAS_SIMULACION][NUM_NODOS]; 
    
    // Estado de los nodos
    float presion_actual[NUM_NODOS]; // Para  "Consultar"
    
    // Estadísticas Globales 
    int total_litros_consumidos;     
    int contador_amonestaciones;     // Por errores de flujo
    int solicitudes_atendidas;
    int consumos_criticos_validados; //
} MemoriaSistema;

// --- Unión necesaria para semáforos en C99 ---
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// --- Prototipos de funciones
void ejecutar_usuario(int id, MemoriaSistema *sistema, int sem_id);
void ejecutar_auditor(MemoriaSistema *sistema, int sem_id);
void imprimir_reporte_final(MemoriaSistema *sistema);

#endif
/*
 * =========================================================================================
 * Archivo:    agentes.c
 * Descripción: Lógica de los hilos (Usuarios y Auditor).
 * =========================================================================================
 */

#include "../include/agentes.h"
#include "../include/nucleo_flujo.h"
#include "../include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <stdint.h> 
#include <pthread.h> 

/* Conversión de tiempo: 30s reales = 12 horas simuladas */
#define SEG_POR_HORA_SIM  ((double)DURACION_SIMULACION / 12.0)

/* ── Estructuras y Variables para la Cola del Auditor (Buffer Circular) ── */
typedef struct {
    int uid;        // Quién generó la alerta
    double litros;  // Cuánto consumió
    int nodo;       // En qué válvula
} EventoCritico;

/* Buffer circular para almacenar eventos críticos */
static EventoCritico cola_criticos[TAM_COLA_AUDITOR];
static int head = 0, tail = 0;

/* Semáforos para el patrón Productor-Consumidor */
static sem_t sem_huecos;    // Cuenta espacios libres en el buffer (Inicial = TAM_COLA)
static sem_t sem_elementos; // Cuenta elementos disponibles para consumir (Inicial = 0)
static sem_t sem_mutex;     // Exclusión mutua para modificar head/tail del buffer

/* Inicializa los mecanismos de sincronización del Auditor */
void agentes_init(void) {
    if (sem_init(&sem_huecos, 0, TAM_COLA_AUDITOR) != 0) { perror("sem_init huecos"); }
    if (sem_init(&sem_elementos, 0, 0) != 0) { perror("sem_init elementos"); }
    if (sem_init(&sem_mutex, 0, 1) != 0) { perror("sem_init mutex"); }
    head = 0; tail = 0;
}

void agentes_close(void) {
    sem_destroy(&sem_huecos);
    sem_destroy(&sem_elementos);
    sem_destroy(&sem_mutex);
}

/* ════════════════════════════════════════════════════════════════════
 * HILO: agente_usuario
 * Simula el ciclo de vida de un cliente (Residencial o Industrial).
 * ════════════════════════════════════════════════════════════════════ */
void* agente_usuario(void* arg) {
    /* Desempaquetar argumentos y liberar memoria inmediatamente */
    ArgsUsuario *params = (ArgsUsuario *)arg;
    int          uid    = params->uid;
    TipoUsuario  tipo   = params->tipo;
    free(params);

    /* Generar semilla única por hilo para rand_r (Thread-safe) */
    unsigned int semilla = (unsigned int)(uid * 1000 + (int)(intptr_t)pthread_self());
    const char *tipo_str = (tipo == TIPO_INDUSTRIAL) ? "INDUSTRIAL " : "RESIDENCIAL";
    
    pthread_t tid = pthread_self();
    tls_estado_hilo = "INICIANDO";

    log_evento(COL_CYAN, "%s | #%03d creado  [TID: %lu]", tipo_str, uid, (unsigned long)tid);

    /* ── Simulación de llegada aleatoria (06:00 - 18:00) ── */
    tls_estado_hilo = "LLEGANDO";
    int tiempo_llegada_us = rand_r(&semilla) % (DURACION_SIMULACION * 1000000);
    usleep((useconds_t)tiempo_llegada_us);

    /* ── Decisión probabilística de acción (MUTUAMENTE EXCLUYENTE) ── */
    int accion = rand_r(&semilla) % 100;

    /* ─ Caso A (10 %): Cancelación inválida ─ */
    if (accion < 10) {
        tls_estado_hilo = "CANCELANDO";
        int nodo_random = rand_r(&semilla) % NUM_VALVULAS;
        log_evento(COL_RED, "%s | #%03d ⚠  Cancelación inválida → válvula %d", 
                   tipo_str, uid, nodo_random);
        
        // Intentar liberar algo que no es suyo (genera amonestación)
        nucleo_liberar_reserva(uid, nodo_random, 0.0);
    }

    /* ─ Caso B (40 %): Consultar presión (Lectura) ─ */
    /* ¡OJO AQUÍ! El 'else if' asegura que si entró al Caso A, NO entre aquí */
    else if (accion < 50) {
        tls_estado_hilo = "CONSULTANDO";
        int nodo_random = rand_r(&semilla) % NUM_VALVULAS;
        log_evento(COL_CYAN, "%s | #%03d 🔍 Consultando presión válvula %d", 
                   tipo_str, uid, nodo_random);
        
        // Llamada a función de lectura (no bloquea a otros lectores)
        nucleo_consultar_presion(uid, nodo_random);
    }

    /* ─ Caso C (50 %): Reservar y Consumir (Escritura) ─ */
    else {
        tls_estado_hilo = "ESPERANDO";
        int  nodo_objetivo = rand_r(&semilla) % NUM_VALVULAS;
        bool reservado     = false;

        /* Paso 1: Intentar reservar (Polling con espera) */
        while (!reservado) {
            reservado = nucleo_intentar_reserva(uid, nodo_objetivo);
            if (!reservado) {
                log_evento(COL_YELLOW, "%s | #%03d ⏳ Esperando asignación válvula %d...", 
                           tipo_str, uid, nodo_objetivo);
                usleep(150000); // Espera breve antes de reintentar
            }
        }

        /* Paso 2: Consumir (Simulación de tiempo de uso) */
        log_evento(COL_GREEN, "%s | #%03d ✅ Válvula %d asignada. Consumiendo...", 
                   tipo_str, uid, nodo_objetivo);

        tls_estado_hilo = "CONSUMIENDO";
        usleep(150000); // Tiempo de consumo simulado

        /* Generar consumo aleatorio (300 - 700 Litros) */
        double litros = 300.0 + (double)(rand_r(&semilla) % 400);

        tls_estado_hilo = "LIBERANDO";
        const char *pago = (litros > LITROS_CRITICOS) ? "💲 Excedente" : "✔ Normal";
        
        log_evento(COL_YELLOW, "%s | #%03d 💧 %.1f L consumidos (%s). Liberando.", 
                   tipo_str, uid, litros, pago);

        /* Paso 3: Reportar al Auditor si es consumo crítico (Productor) */
        if (litros > LITROS_CRITICOS) {
            tls_estado_hilo = "ENCOLANDO";
            
            sem_wait(&sem_huecos);      // Esperar espacio en buffer
            sem_wait(&sem_mutex);       // Exclusión mutua para buffer
            
            cola_criticos[tail].uid = uid;
            cola_criticos[tail].litros = litros;
            cola_criticos[tail].nodo = nodo_objetivo;
            tail = (tail + 1) % TAM_COLA_AUDITOR;
            
            sem_post(&sem_mutex);
            sem_post(&sem_elementos);   // Avisar al auditor que hay un item
            
            tls_estado_hilo = "LIBERANDO";
        }

        /* Paso 4: Liberar válvula (Escritura) */
        nucleo_liberar_reserva(uid, nodo_objetivo, litros);
    }

    tls_estado_hilo = "TERMINADO";
    return NULL;
}

/* ════════════════════════════════════════════════════════════════════
 * HILO: agente_auditor
 * Consumidor que procesa las alertas de consumo crítico.
 * ════════════════════════════════════════════════════════════════════ */
void* agente_auditor(void* arg) {
    (void)arg; // No usamos argumentos

    log_evento(COL_MAGENTA COL_BOLD, "AUDITOR  | 🟢 Supervisión ACTIVA (Productor-Consumidor)");

    int senales_procesadas = 0;
    char buf_estado[64];

    while (1) {
        global_estado_auditor = "DURMIENDO";
        tls_estado_hilo = "ESPERANDO";

        /* Esperar hasta que haya elementos en la cola (Sleep si está vacía) */
        sem_wait(&sem_elementos);

        /* Sección Crítica: Extraer del buffer */
        sem_wait(&sem_mutex);
        EventoCritico ev = cola_criticos[head];
        head = (head + 1) % TAM_COLA_AUDITOR;
        sem_post(&sem_mutex);
        sem_post(&sem_huecos); // Avisar que hay un hueco libre

        /* Procesamiento de la alerta */
        senales_procesadas++;
        sprintf(buf_estado, "VALIDANDO SEÑAL #%d", senales_procesadas);
        global_estado_auditor = buf_estado;
        tls_estado_hilo = "VALIDANDO";

        log_evento(COL_MAGENTA, "AUDITOR  | Evaluando sobreconsumo de %.1f L (Usuario #%d)", 
                   ev.litros, ev.uid);
        
        usleep(150000); // Simula tiempo de validación administrativa
    }

    return NULL;
}
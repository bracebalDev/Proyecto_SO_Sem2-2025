/*
 * =========================================================================================
 * Archivo:    nucleo_flujo.c
 * Descripción: Implementación del "Motor de Concurrencia" de Eco-Flow.
 * =========================================================================================
 */

#include "../include/nucleo_flujo.h"
#include "../include/logger.h"
#include "definiciones.h" 


static NodoFlujo nodos[NUM_VALVULAS];

/* Semáforo para proteger las estadísticas globales (Atomicidad) */
static sem_t sem_stats;

/* ── Estadísticas globales ──────────────────────────────────────────── */
static double total_litros_procesados = 0.0;
static int    total_amonestaciones    = 0;
static int    senales_criticas        = 0;
static int    senales_estandar        = 0;
static int    total_consultas         = 0; 

/* ── API pública ────────────────────────────────────────────────────── */

void nucleo_iniciar_sistema(void) {
    // Inicializar semáforo de estadísticas en 1 (Binary Semaphore = Mutex)
    sem_init(&sem_stats, 0, 1);

    for (int i = 0; i < NUM_VALVULAS; i++) {
        nodos[i].ocupado = false;
        nodos[i].propietario_uid = -1;
        nodos[i].cont_lectores = 0;
        
        // Inicializar semáforos de cada nodo
        sem_init(&nodos[i].sem_escritura, 0, 1); // 1 = Libre para escribir
        sem_init(&nodos[i].sem_lectores, 0, 1);  // 1 = Libre para actualizar contador
    }
    total_consultas         = 0;
    total_litros_procesados = 0.0;
    total_amonestaciones    = 0;
    senales_criticas        = 0;
    senales_estandar        = 0;
   
    log_evento(COL_WHITE COL_BOLD, "SISTEMA  | Iniciado con SEMÁFOROS POSIX.");
}

void nucleo_apagar_sistema(void) {
    for (int i = 0; i < NUM_VALVULAS; i++) {
        sem_destroy(&nodos[i].sem_escritura);
        sem_destroy(&nodos[i].sem_lectores);
    }
    sem_destroy(&sem_stats);
}

/* ─── R1: Exclusión Mutua — Reservar (ESCRITOR) ────────────────────── */
bool nucleo_intentar_reserva(int uid, int nodo_idx) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return false;

    /* Intentar bajar el semáforo de escritura (trywait no bloquea si está ocupado) */
    if (sem_trywait(&nodos[nodo_idx].sem_escritura) == 0) {
        
        // Entramos a la Sección Crítica
        if (nodos[nodo_idx].ocupado) {
            // Ya estaba ocupado (lógica defensiva)
            sem_post(&nodos[nodo_idx].sem_escritura);
            return false;
        }

        nodos[nodo_idx].ocupado = true;
        nodos[nodo_idx].propietario_uid = uid;
        
        sem_post(&nodos[nodo_idx].sem_escritura); // Salir SC
        
        // Log informativo (fuera de la SC para no bloquear)
        log_evento(COL_GREEN, "VÁLVULAS | %d asignada a usuario #%d", nodo_idx, uid);
        return true;
    }
    
    return false; // Estaba ocupado por otro escritor o lectores activos
}

/* ─── R1: Liberar (ESCRITOR) ───────────────────────────────────────── */
void nucleo_liberar_reserva(int uid, int nodo_idx, double litros) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return;

    sem_wait(&nodos[nodo_idx].sem_escritura); // Bloqueo obligatorio (Wait)

    if (nodos[nodo_idx].ocupado && nodos[nodo_idx].propietario_uid == uid) {
        nodos[nodo_idx].ocupado = false;
        nodos[nodo_idx].propietario_uid = -1;

        // Actualizar stats (protegido por semáforo de estadísticas)
        sem_wait(&sem_stats);
        total_litros_procesados += litros;
        if (litros > LITROS_CRITICOS) senales_criticas++;
        else senales_estandar++;
        sem_post(&sem_stats);

        sem_post(&nodos[nodo_idx].sem_escritura); // Liberar nodo

        log_evento(COL_YELLOW, "VÁLVULAS | %d LIBERADA (%.1f L)", nodo_idx, litros);
    } else {
        // R3: Intento ilegal de liberación
        sem_post(&nodos[nodo_idx].sem_escritura);
        nucleo_sancionar_usuario();
    }
}

/* ─── R2: Consultar presión (LECTOR) ───────────────────────────────── */
void nucleo_consultar_presion(int uid, int nodo_idx) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return;

    // 1. Entrada de lector (Protocolo de entrada)
    sem_wait(&nodos[nodo_idx].sem_lectores);
    nodos[nodo_idx].cont_lectores++;
    if (nodos[nodo_idx].cont_lectores == 1) {
        // El primer lector cierra la puerta a los escritores
        sem_wait(&nodos[nodo_idx].sem_escritura);
    }
    sem_post(&nodos[nodo_idx].sem_lectores);

    // 2. Sección Crítica de Lectura (Simultánea)
    bool libre = !nodos[nodo_idx].ocupado;

    // 3. Salida de lector (Protocolo de salida)
    sem_wait(&nodos[nodo_idx].sem_lectores);
    nodos[nodo_idx].cont_lectores--;
    if (nodos[nodo_idx].cont_lectores == 0) {
        // El último lector abre la puerta a los escritores
        sem_post(&nodos[nodo_idx].sem_escritura);
    }
    sem_post(&nodos[nodo_idx].sem_lectores);

    // 4. Registrar la estadística de consulta
    sem_wait(&sem_stats);
    total_consultas++; 
    sem_post(&sem_stats);

    log_evento(COL_CYAN, "PRESIÓN  | Usuario #%d vio válvula %d: %s", 
               uid, nodo_idx, libre ? "LIBRE" : "OCUPADA");
}

void nucleo_sancionar_usuario(void) {
    sem_wait(&sem_stats);
    total_amonestaciones++;
    sem_post(&sem_stats);
}

void nucleo_get_stats(EstadisticasFlujo *dest) {
    if (!dest) return;
    sem_wait(&sem_stats);
    dest->total_litros      = total_litros_procesados;
    dest->amonestaciones    = total_amonestaciones;
    dest->entregas_criticas = senales_criticas;
    dest->entregas_estandar = senales_estandar;
    dest->total_consultas   = total_consultas;
    sem_post(&sem_stats);
}

/* ─── Funciones Auxiliares para Logger y Telemetría ────────────────── */

// Devuelve cuántas válvulas están libres en este instante (Snapshot seguro)
int nucleo_valvulas_libres(void) {
    int libres = 0;
    for (int i = 0; i < NUM_VALVULAS; i++) {
        sem_wait(&nodos[i].sem_lectores);
        if (!nodos[i].ocupado) libres++;
        sem_post(&nodos[i].sem_lectores);
    }
    return libres;
}

// Genera una cadena visual del estado tipo: [#-##----#-]
void nucleo_get_estado_valvulas_str(char *buf) {
    if (!buf) return;
    
    buf[0] = '[';
    for (int i = 0; i < NUM_VALVULAS; i++) {
        sem_wait(&nodos[i].sem_lectores);
        buf[i+1] = nodos[i].ocupado ? '#' : '-';
        sem_post(&nodos[i].sem_lectores);
    }
    buf[NUM_VALVULAS + 1] = ']';
    buf[NUM_VALVULAS + 2] = '\0';
}
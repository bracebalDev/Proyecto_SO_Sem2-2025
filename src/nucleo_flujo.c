/* Archivo: nucleo_flujo.c
 *
 * Núcleo de sincronización del sistema Eco-Flow 2026.
 *
 * Reglas de concurrencia implementadas:
 *   R1 — Exclusión Mutua   : nucleo_intentar_reserva / nucleo_liberar_reserva
 *                            usan pthread_rwlock en modo ESCRITURA.
 *   R2 — Lectores/Escritores: nucleo_consultar_presion usa modo LECTURA,
 *                            permitiendo consultas simultáneas sin bloquear.
 *   R3 — Cancelación inválida: si un usuario intenta liberar un nodo que no
 *                            le pertenece recibe una amonestación.
 */
#include "../include/nucleo_flujo.h"
#include "../include/logger.h"
#include <stdio.h>

/* ── Estructura de cada válvula (nodo de flujo) ─────────────────────── */
typedef struct {
    bool ocupado;
    int  propietario_uid;
    pthread_rwlock_t rwlock;   /* Protege el estado de ESTE nodo que usa protocolo Lectores-Escritores */
} NodoFlujo;

static NodoFlujo nodos[NUM_VALVULAS];

/* ── Estadísticas globales (Estado inicial) ──────────────────────────────────────────── */
static double total_litros_procesados = 0.0;
static int    total_amonestaciones    = 0;
static int    senales_criticas        = 0;
static int    senales_estandar        = 0;

/* Mutex que protege las estadísticas (separado de los rwlock de nodos) */
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Helpers internos ───────────────────────────────────────────────── */

/**
 * Cuenta cuántos nodos están libres SIN adquirir ningún lock propio
 * (el llamador debe ya tener el contexto adecuado, o se usa en paths
 * donde la inconsistencia momentánea es aceptable para el log).
 * Para la API pública se usa nucleo_valvulas_libres() con read-lock.
 */
static int _contar_libres_unsafe(void) {
    int libres = 0;
    for (int i = 0; i < NUM_VALVULAS; i++) {
        if (!nodos[i].ocupado) libres++;
    }
    return libres;
}

/* ── API pública ────────────────────────────────────────────────────── */

// Procedimiento para iniciar el sistema
void nucleo_iniciar_sistema(void) {
    total_litros_procesados = 0.0;
    total_amonestaciones = 0;
    senales_criticas = 0;
    senales_estandar = 0;

    for (int i = 0; i < NUM_VALVULAS; i++) {
        nodos[i].ocupado          = false;
        nodos[i].propietario_uid  = -1;
        pthread_rwlock_init(&nodos[i].rwlock, NULL);
    }
    log_evento(COL_WHITE COL_BOLD,
               "SISTEMA  | Hay %d válvulas disponibles, TODAS libres al inicio",
               NUM_VALVULAS);
}

// Procedimiento para apagar el sistema
void nucleo_apagar_sistema(void) {
    for (int i = 0; i < NUM_VALVULAS; i++) {
        pthread_rwlock_destroy(&nodos[i].rwlock);
    }
    pthread_mutex_destroy(&stats_mutex);
}

/* ─── R1: Exclusión Mutua — Reservar ───────────────────────────────── */
bool nucleo_intentar_reserva(int uid, int nodo_idx) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return false;

    pthread_rwlock_wrlock(&nodos[nodo_idx].rwlock);

    if (nodos[nodo_idx].ocupado) {
        pthread_rwlock_unlock(&nodos[nodo_idx].rwlock);
        return false;    /* Nodo ocupado → el agente reintentará */
    }

    nodos[nodo_idx].ocupado         = true;
    nodos[nodo_idx].propietario_uid = uid;

    pthread_rwlock_unlock(&nodos[nodo_idx].rwlock);

    /* Log del nuevo estado de válvulas */
    int libres = _contar_libres_unsafe();
    log_evento(COL_GREEN,
               "VÁLVULAS | %2d/%d libres — Válvula %d ASIGNADA al usuario #%d",
               libres, NUM_VALVULAS, nodo_idx, uid);

    return true;
}

/* ─── R1: Exclusión Mutua — Liberar (Protocolo de liberación) ────────────────────────────────── */
void nucleo_liberar_reserva(int uid, int nodo_idx, double litros) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return;

    pthread_rwlock_wrlock(&nodos[nodo_idx].rwlock);

    if (nodos[nodo_idx].ocupado && nodos[nodo_idx].propietario_uid == uid) {
        nodos[nodo_idx].ocupado         = false;
        nodos[nodo_idx].propietario_uid = -1;

        /* Acumular estadísticas de forma atómica */
        pthread_mutex_lock(&stats_mutex);
        total_litros_procesados += litros;
        if (litros > LITROS_CRITICOS) {
            senales_criticas++;
        } else {
            senales_estandar++;
        }
        pthread_mutex_unlock(&stats_mutex);

        pthread_rwlock_unlock(&nodos[nodo_idx].rwlock);

        /* Log de liberación con tipo de señal */
        int libres = _contar_libres_unsafe();
        const char *tipo  = (litros > LITROS_CRITICOS) ? "🔴 CRÍTICA" : "🟢 ESTÁNDAR";
        log_evento(COL_YELLOW,
                   "VÁLVULAS | %2d/%d libres — Válvula %d LIBERADA por usuario #%d "
                   "[%.1f L, señal %s]",
                   libres, NUM_VALVULAS, nodo_idx, uid, litros, tipo);
    } else {
        /* R3: el usuario no posee este nodo → amonestación */
        pthread_rwlock_unlock(&nodos[nodo_idx].rwlock);
        nucleo_sancionar_usuario();
    }
}

/* ─── R2: Lectores/Escritores — Consultar presión ──────────────────── */
void nucleo_consultar_presion(int uid, int nodo_idx) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return;

    pthread_rwlock_rdlock(&nodos[nodo_idx].rwlock);
    bool libre = !nodos[nodo_idx].ocupado;
    pthread_rwlock_unlock(&nodos[nodo_idx].rwlock);

    log_evento(COL_CYAN,
               "PRESIÓN  | Usuario #%d consultó válvula %d → %s",
               uid, nodo_idx, libre ? "LIBRE" : "OCUPADA");
}

/* ─── R3: Amonestación ──────────────────────────────────────────────── */
void nucleo_sancionar_usuario(void) {
    pthread_mutex_lock(&stats_mutex);
    total_amonestaciones++;
    pthread_mutex_unlock(&stats_mutex);
}

/* ─── Consultas de estado ────────────────────────────────────────────── */
int nucleo_valvulas_libres(void) {
    int libres = 0;
    for (int i = 0; i < NUM_VALVULAS; i++) {
        pthread_rwlock_rdlock(&nodos[i].rwlock);
        if (!nodos[i].ocupado) libres++;
        pthread_rwlock_unlock(&nodos[i].rwlock);
    }
    return libres;
}

void nucleo_get_stats(EstadisticasFlujo *dest) {
    if (!dest) return;
    pthread_mutex_lock(&stats_mutex);
    dest->total_litros      = total_litros_procesados;
    dest->amonestaciones    = total_amonestaciones;
    dest->entregas_criticas = senales_criticas;
    dest->entregas_estandar = senales_estandar;
    pthread_mutex_unlock(&stats_mutex);
}

void nucleo_get_estado_valvulas_str(char *buf) {
    buf[0] = '[';
    for (int i = 0; i < NUM_VALVULAS; i++) {
        /* Lectura atómica superficial para representación visual rápida */
        buf[i+1] = nodos[i].ocupado ? '#' : '-';
    }
    buf[NUM_VALVULAS + 1] = ']';
    buf[NUM_VALVULAS + 2] = '\0';
}

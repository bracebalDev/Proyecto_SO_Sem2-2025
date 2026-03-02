/* Archivo: agentes.c
 *
 * Define los hilos del sistema:
 *   · agente_usuario  — Simula un Nodo de Consumo Residencial o Industrial.
 *   · agente_auditor  — Supervisor que reporta métricas por hora simulada.
 *
 * Conversión de tiempo:
 *   DURACION_SIMULACION s reales  ≡  12 horas (06:00-18:00)
 *   → 1 hora simulada = SEG_POR_HORA_SIM = DURACION_SIMULACION / 12.0 s reales
 *
 * Thread-safety de rand():
 *   Cada hilo usa rand_r(&semilla_local) con semilla derivada de su uid
 *   para evitar condiciones de carrera sobre el estado global del PRNG.
 */
#include "../include/agentes.h"
#include "../include/nucleo_flujo.h"
#include "../include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // Para la función usleep
#include <stdint.h> // Para manejo de enteros de tamaño variable
#include <pthread.h> // Para manejo de los hilos

/* Segundos reales que equivalen a 1 hora simulada */
#define SEG_POR_HORA_SIM  ((double)DURACION_SIMULACION / 12.0)

/* ── Estructuras y Variables para la Cola del Auditor ────────────── */
typedef struct {
    int uid;
    double litros;
    int nodo;
} EventoCritico;

static EventoCritico cola_criticos[TAM_COLA_AUDITOR];
static int head = 0, tail = 0;
static sem_t sem_huecos;
static sem_t sem_elementos;
static sem_t sem_mutex;

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
 *  HILO: agente_usuario
 *
 *  arg: puntero a ArgsUsuario* reservado en el heap por main.
 *       El hilo es responsable de liberar ese bloque con free().
 * ════════════════════════════════════════════════════════════════════ */
void* agente_usuario(void* arg) {
    /* Desempaquetar argumentos y liberar inmediatamente la memoria de paso */
    ArgsUsuario *params = (ArgsUsuario *)arg;
    int          uid    = params->uid;
    TipoUsuario  tipo   = params->tipo;
    free(params);

    /* Semilla por-hilo: evita condición de carrera sobre rand() global */
    unsigned int semilla = (unsigned int)(uid * 1000 + (int)(intptr_t)pthread_self());

    /* Etiqueta del tipo de nodo (Residencial / Industrial) */
    const char *tipo_str = (tipo == TIPO_INDUSTRIAL) ? "INDUSTRIAL " : "RESIDENCIAL";

    pthread_t tid = pthread_self();
    tls_estado_hilo = "INICIANDO";

    log_evento(COL_CYAN,
               "%s | #%03d creado  [TID: %lu]",
               tipo_str, uid, (unsigned long)tid);

    /* ── Llegada aleatoria para generar ALTA contención ── */
    tls_estado_hilo = "LLEGANDO";
    int tiempo_llegada_us = (int)(rand_r(&semilla) % (2000)) * 1000; /* max 2s */
    usleep((useconds_t)tiempo_llegada_us);

    /* ── Decisión probabilística de acción ── */
    int accion = rand_r(&semilla) % 100;

    /* ─ Caso A (10 %): Cancelación inválida ──────────────────────────── */
    if (accion < 10) {
        tls_estado_hilo = "CANCELANDO";
        int nodo_random = rand_r(&semilla) % NUM_VALVULAS;
        log_evento(COL_RED,
                   "%s | #%03d ⚠  Cancelación inválida → válvula %d  [sin reserva activa]",
                   tipo_str, uid, nodo_random);
        /* R3: el núcleo detectará que no es dueño y emitirá amonestación */
        nucleo_liberar_reserva(uid, nodo_random, 0.0);
    }

    /* ─ Caso B (40 %): Consultar presión disponible ────────────────────
     *   R2: read-lock en el núcleo → múltiples consultas simultáneas OK  */
    else if (accion < 50) {
        tls_estado_hilo = "CONSULTANDO";
        int nodo_random = rand_r(&semilla) % NUM_VALVULAS;
        log_evento(COL_CYAN,
                   "%s | #%03d 🔍 Consultando presión disponible en válvula %d",
                   tipo_str, uid, nodo_random);
        nucleo_consultar_presion(uid, nodo_random);
    }

    /* ─ Caso C (50 %): Reservar → Consumir → Pagar/Liberar ─────────────
     *   R1: write-lock durante reserva y liberación                       */
    else {
        tls_estado_hilo = "ESPERANDO";
        int  nodo_objetivo = rand_r(&semilla) % NUM_VALVULAS;
        bool reservado     = false;

        /* Acción 1: Esperar asignación (polling pasivo) */
        while (!reservado) {
            reservado = nucleo_intentar_reserva(uid, nodo_objetivo);
            if (!reservado) {
                log_evento(COL_YELLOW,
                           "%s | #%03d ⏳ Esperando asignación — válvula %d ocupada, reintentando...",
                           tipo_str, uid, nodo_objetivo);
                usleep(150000); /* 150 ms entre intentos */
            }
        }

        /* Acción 2: Consumir agua (trabaja durante 1 hora simulada exacta) */
        log_evento(COL_GREEN,
                   "%s | #%03d ✅ Válvula %d asignada. Consumiendo agua (~1 h simulada)...",
                   tipo_str, uid, nodo_objetivo);

        tls_estado_hilo = "CONSUMIENDO";
        usleep(150000); /* 150ms reales para notar contención pero sin bloquear todo de más */

        /* Consumo aleatorio: 300-699 L (puede superar umbral crítico de 500 L)
         * R4: si > LITROS_CRITICOS, el núcleo lo registra como señal crítica */
        double litros = 300.0 + (double)(rand_r(&semilla) % 400);

        tls_estado_hilo = "LIBERANDO";

        /* Acción 5: Pagar tarifa de excedente (si aplica) y liberar el nodo */
        const char *pago = (litros > LITROS_CRITICOS)
                         ? "💲 Tarifa de excedente aplicada"
                         : "✔ Sin excedente";
        log_evento(COL_YELLOW,
                   "%s | #%03d 💧 %.1f L consumidos en válvula %d. %s → Liberando.",
                   tipo_str, uid, litros, nodo_objetivo, pago);

        /* ENCOLAMOS a Auditor si se excede */
        if (litros > LITROS_CRITICOS) {
            tls_estado_hilo = "ENCOLANDO";
            sem_wait(&sem_huecos);
            sem_wait(&sem_mutex);
            cola_criticos[tail].uid = uid;
            cola_criticos[tail].litros = litros;
            cola_criticos[tail].nodo = nodo_objetivo;
            tail = (tail + 1) % TAM_COLA_AUDITOR;
            sem_post(&sem_mutex);
            sem_post(&sem_elementos);
            tls_estado_hilo = "LIBERANDO";
        }

        nucleo_liberar_reserva(uid, nodo_objetivo, litros);
    }

    tls_estado_hilo = "TERMINADO";
    return NULL;
}

/* ════════════════════════════════════════════════════════════════════
 *  HILO: agente_auditor
 *
 *  Supervisa el sistema durante las 12 horas simuladas.
 *  Cada iteración duerme SEG_POR_HORA_SIM segundos reales (= DURACION_SIMULACION/12)
 *  para alinearse con la duración total de la simulación.
 * ════════════════════════════════════════════════════════════════════ */
void* agente_auditor(void* arg) {
    (void)arg;

    log_evento(COL_MAGENTA COL_BOLD,
               "AUDITOR  | 🟢 Inicio de supervisión ACTIVA del sistema (Productor-Consumidor con Semáforos)");

    int senales_procesadas = 0;
    char buf_estado[64];

    while (1) {
        global_estado_auditor = "DURMIENDO";
        tls_estado_hilo = "ESPERANDO";

        /* El hilo se bloquea hasta recibir una señal que lo deje entrar */
        sem_wait(&sem_elementos);

        sem_wait(&sem_mutex);
        EventoCritico ev = cola_criticos[head];
        head = (head + 1) % TAM_COLA_AUDITOR;
        sem_post(&sem_mutex);
        sem_post(&sem_huecos);

        senales_procesadas++;
        sprintf(buf_estado, "VALIDANDO SEÑAL #%d", senales_procesadas);
        global_estado_auditor = buf_estado;
        tls_estado_hilo = "VALIDANDO";

        log_evento(COL_MAGENTA,
                   "AUDITOR  | Evaluando sobreconsumo de %.1f L del usuario #%d en válvula %d",
                   ev.litros, ev.uid, ev.nodo);
        
        /* Simula procesamiento / validación */
        usleep(150000); 
    }

    return NULL;
}

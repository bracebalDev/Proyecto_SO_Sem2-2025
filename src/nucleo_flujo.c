/*
 * =========================================================================================
 * Archivo:    nucleo_flujo.c
 * Descripción: Implementación del "Motor de Concurrencia" de Eco-Flow.
 * Gestiona la sincronización de nodos de flujo mediante semáforos POSIX,
 * implementando el patrón Lectores/Escritores estricto.
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
static int    total_reservas_exitosas = 0;
static double tiempo_espera_global    = 0.0;
static double tiempo_espera_maximo    = 0.0;

/* ── API pública ────────────────────────────────────────────────────── */

void nucleo_iniciar_sistema(void) {
    /* Inicializar semáforo de estadísticas en 1 (Semáforo binario = Mutex) */
    sem_init(&sem_stats, 0, 1);

    for (int i = 0; i < NUM_VALVULAS; i++) {
        nodos[i].ocupado = false;
        nodos[i].propietario_uid = -1;
        nodos[i].cont_lectores = 0;
        
        /* Inicializar semaforos de cada nodo */
        sem_init(&nodos[i].sem_escritura, 0, 1); /* 1 = Libre para escribir */
        sem_init(&nodos[i].sem_lectores, 0, 1);  /* 1 = Libre para actualizar contador */
        sem_init(&nodos[i].sem_turno, 0, 1);     /* 1 = Turno libre (fairness L/E) */
    }
    total_consultas         = 0;
    total_litros_procesados = 0.0;
    total_amonestaciones    = 0;
    senales_criticas        = 0;
    senales_estandar        = 0;
    total_reservas_exitosas = 0;
    tiempo_espera_global    = 0.0;
    tiempo_espera_maximo    = 0.0;
   
    log_evento(COL_WHITE COL_BOLD, "SISTEMA  | Iniciado con SEMÁFOROS POSIX.");
}

void nucleo_apagar_sistema(void) {
    for (int i = 0; i < NUM_VALVULAS; i++) {
        sem_destroy(&nodos[i].sem_escritura);
        sem_destroy(&nodos[i].sem_lectores);
        sem_destroy(&nodos[i].sem_turno);
    }
    sem_destroy(&sem_stats);
}

/* ═══════════════════════════════════════════════════════════════════════
 * R1: Exclusión Mutua — Reservar nodo (ESCRITOR bloqueante)
 *
 * PROTOCOLO DE NEGOCIACIÓN (Adquisición del candado):
 * El hilo ejecuta sem_wait sobre el semáforo de escritura del nodo.
 * Si el nodo está libre (semáforo en 1), el hilo lo adquiere de
 * inmediato. Si está ocupado (semáforo en 0), el hilo se BLOQUEA
 * aquí sin consumir CPU (a diferencia del polling anterior).
 *
 * SECCIÓN CRÍTICA:
 * Se marca el nodo como ocupado y se registra el propietario.
 *
 * PROTOCOLO DE LIBERACIÓN:
 * NO se hace sem_post aquí. El candado permanece retenido mientras
 * el hilo consume agua, garantizando exclusión mutua estricta.
 * El sem_post se realizará en nucleo_liberar_reserva.
 * ═══════════════════════════════════════════════════════════════════════ */
bool nucleo_reservar_nodo(int uid, int nodo_idx) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return false;

    /* ── Protocolo de negociacion con fairness (sem_turno) ──
     * 1. sem_wait(sem_turno): bloquea la entrada de nuevos lectores.
     * 2. sem_wait(sem_escritura): espera a que los lectores actuales salgan.
     * 3. sem_post(sem_turno): libera el turno para que otros puedan entrar.
     * Esto evita starvation de escritores ante flujo continuo de lectores. */
    sem_wait(&nodos[nodo_idx].sem_turno);
    sem_wait(&nodos[nodo_idx].sem_escritura);
    sem_post(&nodos[nodo_idx].sem_turno);

    /* ── Inicio de seccion critica ──
     * Solo un hilo puede estar aqui a la vez gracias al semaforo.      */
    nodos[nodo_idx].ocupado = true;
    nodos[nodo_idx].propietario_uid = uid;
    /* ── Fin de seccion critica (candado retenido) ── */

    log_evento(COL_GREEN, "VALVULAS | %d asignada a usuario #%d", nodo_idx, uid);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 * R1: Liberar nodo (ESCRITOR)
 *
 * PROTOCOLO DE NEGOCIACIÓN:
 * - Caso normal (fue_cancelacion == false): el hilo YA posee el
 * candado desde nucleo_reservar_nodo, así que entra directo.
 * - Caso cancelación (fue_cancelacion == true): el hilo NO tiene
 * el candado (es un intento inválido de liberar un nodo ajeno),
 * así que lo adquiere con sem_wait antes de verificar propiedad.
 * ═══════════════════════════════════════════════════════════════════════ */
void nucleo_liberar_reserva(int uid, int nodo_idx, double litros, bool fue_cancelacion) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return;

    /* Si es una cancelacion (el hilo NO tenia el candado), intenta
     * adquirirlo de forma NO bloqueante con sem_trywait.
     * Si el nodo esta ocupado por otro, la cancelacion es invalida
     * de todas formas, asi que se aplica la amonestacion directamente
     * sin bloquear al hilo innecesariamente.                            */
    if (fue_cancelacion) {
        if (sem_trywait(&nodos[nodo_idx].sem_escritura) != 0) {
            log_evento(COL_RED, "VALVULAS | Cancelacion rechazada en nodo %d (ocupado por otro)", nodo_idx);
            nucleo_sancionar_usuario(uid);
            return;
        }
    }

    /* ── Inicio de sección crítica ──
     * Verificamos que el hilo sea el propietario legítimo del nodo.     */
    if (nodos[nodo_idx].ocupado && nodos[nodo_idx].propietario_uid == uid) {
        /* Liberación válida: el propietario devuelve el nodo */
        nodos[nodo_idx].ocupado = false;
        nodos[nodo_idx].propietario_uid = -1;

        /* Actualizar estadísticas (protegido por semáforo independiente) */
        sem_wait(&sem_stats);
        total_litros_procesados += litros;
        if (litros > LITROS_CRITICOS) senales_criticas++;
        else senales_estandar++;
        sem_post(&sem_stats);

        /* ── Protocolo de liberación ──
         * Se ejecuta el sem_post que quedó pendiente desde la reserva.
         * Esto despierta al siguiente hilo que esté esperando el nodo.  */
        sem_post(&nodos[nodo_idx].sem_escritura);

        log_evento(COL_YELLOW, "VÁLVULAS | %d LIBERADA (%.1f L)", nodo_idx, litros);
    } else {
        /* ── Intento ilegal de liberación (amonestación) ──
         * El usuario intentó liberar un nodo que no le pertenece.
         * IMPORTANTE: Debemos soltar el candado que agarramos (ya sea
         * por la cancelación o porque algo falló) para no hacer deadlock. */
        sem_post(&nodos[nodo_idx].sem_escritura);
        nucleo_sancionar_usuario(uid);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * R2: Consultar presión (LECTOR) — Patrón Lectores/Escritores
 *
 * PROTOCOLO DE NEGOCIACIÓN (Entrada de lector):
 * Se usa un semáforo de lectores para proteger el contador.
 * El primer lector bloquea a los escritores (sem_wait en escritura).
 * ═══════════════════════════════════════════════════════════════════════ */
void nucleo_consultar_presion(int uid, int nodo_idx) {
    if (nodo_idx < 0 || nodo_idx >= NUM_VALVULAS) return;

    /* ── Entrada del lector CON fairness ──
     * sem_turno actua como torniquete: si un escritor esta esperando,
     * el lector se bloquea aqui en lugar de colarse al contador.
     * Esto evita starvation de escritores ante flujo continuo de lectores. */
    sem_wait(&nodos[nodo_idx].sem_turno);
    sem_post(&nodos[nodo_idx].sem_turno);

    sem_wait(&nodos[nodo_idx].sem_lectores);
    nodos[nodo_idx].cont_lectores++;
    if (nodos[nodo_idx].cont_lectores == 1) {
        /* El primer lector cierra la puerta a los escritores */
        sem_wait(&nodos[nodo_idx].sem_escritura);
    }
    sem_post(&nodos[nodo_idx].sem_lectores);

    /* ── Sección crítica de lectura (simultánea entre lectores) ── */
    bool libre = !nodos[nodo_idx].ocupado;

    /* Salida del lector */
    sem_wait(&nodos[nodo_idx].sem_lectores);
    nodos[nodo_idx].cont_lectores--;
    if (nodos[nodo_idx].cont_lectores == 0) {
        /* El último lector abre la puerta a los escritores */
        sem_post(&nodos[nodo_idx].sem_escritura);
    }
    sem_post(&nodos[nodo_idx].sem_lectores);

    /* Registrar la estadística de consulta (sección crítica aparte) */
    sem_wait(&sem_stats);
    total_consultas++; 
    sem_post(&sem_stats);

    log_evento(COL_CYAN, "PRESIÓN  | Usuario #%d vio válvula %d: %s", 
               uid, nodo_idx, libre ? "LIBRE" : "OCUPADA");
}

void nucleo_sancionar_usuario(int uid) {
    /* ── Sección crítica: actualizar contador de amonestaciones ── */
    sem_wait(&sem_stats);
    total_amonestaciones++;
    sem_post(&sem_stats);
    log_evento(COL_RED COL_BOLD, "SISTEMA  | Sanción aplicada al usuario #%d", uid);
}

void nucleo_get_stats(EstadisticasFlujo *dest) {
    if (!dest) return;
    /* ── Sección crítica: snapshot atómico de todas las estadísticas ── */
    sem_wait(&sem_stats);
    dest->total_litros      = total_litros_procesados;
    dest->amonestaciones    = total_amonestaciones;
    dest->entregas_criticas = senales_criticas;
    dest->entregas_estandar = senales_estandar;
    dest->total_consultas   = total_consultas;
    dest->total_reservas    = total_reservas_exitosas;
    dest->tiempo_espera_total = tiempo_espera_global;
    dest->tiempo_espera_max = tiempo_espera_maximo;
    sem_post(&sem_stats);
}

void nucleo_registrar_eficiencia(double tiempo_esperado) {
    /* ── Sección crítica: registrar métricas de eficiencia ── */
    sem_wait(&sem_stats);
    tiempo_espera_global = tiempo_espera_global + tiempo_esperado;
    total_reservas_exitosas++;

    /* Se guarda el peor caso (inanición máxima) */
    if (tiempo_esperado > tiempo_espera_maximo) {
        tiempo_espera_maximo = tiempo_esperado;
    }
    sem_post(&sem_stats);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Funciones Auxiliares para Logger y Telemetría
 * * CORRECCIÓN DEADLOCK: Se implementaron Dirty Reads (Lecturas sucias).
 * Al ser `ocupado` un tipo primitivo booleano, su lectura es atómica
 * a nivel de hardware. Evitamos solicitar `sem_lectores` aquí porque
 * chocaría con la interfaz gráfica/logger, causando interbloqueos masivos
 * si un lector activo se duerme y un escritor necesita escribir un log.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Devuelve cuántas válvulas están libres en este instante (snapshot rápido) */
int nucleo_valvulas_libres(void) {
    int libres = 0;
    for (int i = 0; i < NUM_VALVULAS; i++) {
        if (!nodos[i].ocupado) libres++;
    }
    return libres;
}

/* Genera una cadena visual del estado tipo: [#-##----#-] */
void nucleo_get_estado_valvulas_str(char *buf) {
    if (!buf) return;
    
    buf[0] = '[';
    for (int i = 0; i < NUM_VALVULAS; i++) {
        buf[i+1] = nodos[i].ocupado ? '#' : '-';
    }
    buf[NUM_VALVULAS + 1] = ']';
    buf[NUM_VALVULAS + 2] = '\0';
}
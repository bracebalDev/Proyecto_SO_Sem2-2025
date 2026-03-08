/*
 * =========================================================================================
 * Archivo:    agentes.c
 * Descripción: Lógica de los hilos (Usuarios y Auditor).
 *              Implementa el ciclo de vida de cada usuario (consultar, reservar, cancelar)
 *              y el auditor que procesa alertas de consumo crítico.
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
#include <time.h>

/* Conversión de tiempo: los segundos reales representan 12 horas simuladas */
#define SEG_POR_HORA_SIM  ((double)DURACION_SIMULACION / 12.0)

/* ── Estructuras y Variables para la Cola del Auditor (Buffer Circular) ── */
typedef struct {
    int uid;        /* Quién generó la alerta */
    double litros;  /* Cuánto consumió */
    int nodo;       /* En qué válvula */
} EventoCritico;

/* Buffer circular para almacenar eventos críticos */
static EventoCritico cola_criticos[TAM_COLA_AUDITOR];
static int head = 0, tail = 0;

/* ── Semáforos para el patrón Productor-Consumidor ──
 *   sem_huecos:    cuenta espacios libres en el buffer (Inicial = TAM_COLA)
 *   sem_elementos: cuenta elementos disponibles para consumir (Inicial = 0)
 *   sem_mutex:     exclusión mutua para modificar head/tail del buffer     */
static sem_t sem_huecos;
static sem_t sem_elementos;
static sem_t sem_mutex;

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

/* ── Función de despertar al auditor para apagado limpio ──
 * Si el auditor está bloqueado en sem_wait(&sem_elementos),
 * este sem_post lo despierta para que revise la bandera
 * simulacion_activa y termine su bucle ordenadamente.                      */
void agentes_despertar_auditor(void) {
    sem_post(&sem_elementos);
}

/* ════════════════════════════════════════════════════════════════════
 * HILO: agente_usuario
 * Simula el ciclo de vida de un cliente (Residencial o Industrial).
 * ════════════════════════════════════════════════════════════════════ */
void* agente_usuario(void* arg) {
    /* Desempaquetar argumentos sin liberar memoria.
     * La memoria de ArgsUsuario es gestionada por el hilo principal (main),
     * quien la libera tras el pthread_join. Esto evita fugas de memoria
     * causadas por la desincronización entre malloc y free en distintos hilos. */
    ArgsUsuario *params = (ArgsUsuario *)arg;
    int          uid    = params->uid;
    TipoUsuario  tipo   = params->tipo;
    /* NO se hace free(params) aquí — main gestiona la memoria */

    /* Generar semilla única por hilo para rand_r (Thread-safe) */
    unsigned int semilla = (unsigned int)(uid * 1000 + (int)(intptr_t)pthread_self());
    const char *tipo_str = (tipo == TIPO_INDUSTRIAL) ? "INDUSTRIAL " : "RESIDENCIAL";
    
    pthread_t tid = pthread_self();
    tls_estado_hilo = "INICIANDO";

    log_evento(COL_CYAN, "%s | #%03d creado  [TID: %lu]", tipo_str, uid, (unsigned long)tid);

    /* ── Simulación de llegada (Distribución Bimodal: 2 Picos) ──
     * Se simula la llegada de usuarios en dos picos: madrugadores (06:00)
     * y hora punta del mediodía. Esto genera contención natural.           */
    tls_estado_hilo = "LLEGANDO";
    int tiempo_llegada_us = 0;
    
    /* Se lanza una probabilidad de 0 a 99 para separar a los madrugadores de los del mediodía */
    int grupo = rand_r(&semilla) % 100;
    
    if (grupo < 40) {
        /* PICO 1: Las 06:00 AM (40% de los usuarios)
         * Como las 6:00 am es el inicio de la simulación (segundo 0), se obligan
         * a despertar aleatoriamente dentro del primer cuarto del día (06:00 a 09:00). */
        int cuarto_dia_us = (DURACION_SIMULACION * 1000000) / 4;
        tiempo_llegada_us = rand_r(&semilla) % cuarto_dia_us; 
    } else {
        /* PICO 2: El Mediodía (60% de los usuarios)
         * Se usa la suma de dos variables (Distribución Triangular) para que la 
         * inmensa mayoría de este grupo se concentre justo en la mitad de la simulación. */
        int mitad_simulacion_us = (DURACION_SIMULACION * 1000000) / 2;
        tiempo_llegada_us = (rand_r(&semilla) % mitad_simulacion_us) + 
                            (rand_r(&semilla) % mitad_simulacion_us);
    }
    
    /* POSIX exige que usleep sea estrictamente menor a 1,000,000.
     * Si el tiempo calculado es mayor, usamos sleep para los segundos
     * enteros y usleep para el residuo. */
    if (tiempo_llegada_us >= 1000000) {
        sleep(tiempo_llegada_us / 1000000);
        usleep((useconds_t)(tiempo_llegada_us % 1000000));
    } else {
        usleep((useconds_t)tiempo_llegada_us);
    }
    

    /* ── Decisión probabilística de acción (MUTUAMENTE EXCLUYENTE) ── */
    int accion = rand_r(&semilla) % 100;

    /* ═══════════════════════════════════════════════════════════════════
     * Caso A (10%): Cancelación inválida
     * El usuario intenta liberar un nodo que no le pertenece.
     * Se pasa fue_cancelacion = true para que nucleo_liberar_reserva
     * sepa que debe adquirir el candado antes de verificar propiedad.
     * ═══════════════════════════════════════════════════════════════════ */
    if (accion < 10) {
        tls_estado_hilo = "CANCELANDO";
        int nodo_random = rand_r(&semilla) % NUM_VALVULAS;
        log_evento(COL_RED, "%s | #%03d ⚠  Cancelación inválida → válvula %d", 
                   tipo_str, uid, nodo_random);
        
        /* Intentar liberar algo que no es suyo (genera amonestación).
         * fue_cancelacion = true indica que el hilo NO posee el candado. */
        nucleo_liberar_reserva(uid, nodo_random, 0.0, true);
    }

    /* ═══════════════════════════════════════════════════════════════════
     * Caso B (40%): Consultar presión (Lectura)
     * Protocolo de negociación: el lector usa el patrón Lectores/Escritores.
     * ¡OJO! El 'else if' asegura que si entró al Caso A, NO entre aquí.
     * ═══════════════════════════════════════════════════════════════════ */
    else if (accion < 50) {
        tls_estado_hilo = "CONSULTANDO";
        int nodo_random = rand_r(&semilla) % NUM_VALVULAS;
        log_evento(COL_CYAN, "%s | #%03d 🔍 Consultando presión válvula %d", 
                   tipo_str, uid, nodo_random);
        
        /* Llamada a función de lectura (no bloquea a otros lectores) */
        nucleo_consultar_presion(uid, nodo_random);
    }

    /* ═══════════════════════════════════════════════════════════════════
     * Caso C (50%): Reservar y Consumir (Escritura)
     *
     * PROTOCOLO DE NEGOCIACIÓN:
     *   Se llama a nucleo_reservar_nodo que usa sem_wait bloqueante.
     *   El hilo se duerme sin consumir CPU hasta que el nodo esté libre.
     *   Esto elimina el busy-waiting (polling) anterior.
     *
     * MÉTRICAS REALES:
     *   Se usa clock_gettime(CLOCK_MONOTONIC) para medir el tiempo de
     *   espera real del hilo, en lugar de estimar con contadores.
     *
     * PROTOCOLO DE LIBERACIÓN:
     *   fue_cancelacion = false porque el hilo SÍ posee el candado
     *   desde nucleo_reservar_nodo, así que no necesita re-adquirirlo.
     * ═══════════════════════════════════════════════════════════════════ */
    else {
        tls_estado_hilo = "ESPERANDO";
        int  nodo_objetivo = rand_r(&semilla) % NUM_VALVULAS;

        /* Paso 1: Medir el tiempo de espera real con reloj monotónico.
         * Se toma un timestamp ANTES y DESPUÉS de la llamada bloqueante
         * para obtener el tiempo exacto que el SO mantuvo dormido al hilo. */
        struct timespec t_inicio, t_fin;
        clock_gettime(CLOCK_MONOTONIC, &t_inicio);

        /* Llamada bloqueante: el hilo se suspende hasta obtener el nodo.
         * Internamente hace sem_wait (sin polling ni usleep).              */
        nucleo_reservar_nodo(uid, nodo_objetivo);

        clock_gettime(CLOCK_MONOTONIC, &t_fin);

        /* Cálculo del tiempo de espera real (en segundos con decimales) */
        double tiempo_esperado = (double)(t_fin.tv_sec - t_inicio.tv_sec)
                               + (double)(t_fin.tv_nsec - t_inicio.tv_nsec) / 1e9;

        /* FACTOR DE ESCALAMIENTO: Se multiplica el tiempo real de espera
         * por FACTOR_ESCALA (derivado de DURACION_BASE_SEG / DURACION_SIMULACION)
         * para que las estadisticas finales sean coherentes con la escala temporal.
         * Si se cambia DURACION_SIMULACION, el factor se ajusta automaticamente. */
        tiempo_esperado = tiempo_esperado * FACTOR_ESCALA;

        /* Registro del tiempo de espera y suma de reservas exitosas */
        nucleo_registrar_eficiencia(tiempo_esperado);

        /* Paso 2: Consumir (Simulación de tiempo de uso) */
        log_evento(COL_GREEN, "%s | #%03d ✅ Válvula %d asignada. Consumiendo...", 
                   tipo_str, uid, nodo_objetivo);

        tls_estado_hilo = "CONSUMIENDO";
        usleep(USLEEP_CONSUMO);  /* Tiempo de consumo simulado (escalado automatico) */

        /* Generar consumo aleatorio (300 - 700 Litros) */
        double litros = 300.0 + (double)(rand_r(&semilla) % 400);

        tls_estado_hilo = "LIBERANDO";
        const char *pago = (litros > LITROS_CRITICOS) ? "💲 Excedente" : "✔ Normal";
        
        log_evento(COL_YELLOW, "%s | #%03d 💧 %.1f L consumidos (%s). Liberando.", 
                   tipo_str, uid, litros, pago);

        /* Paso 3: Liberar válvula ANTES de encolar al auditor.
         * fue_cancelacion = false: el hilo YA posee el candado.
         *
         * CORRECCIÓN CRÍTICA: Se libera el nodo PRIMERO para que
         * el candado (sem_escritura) no quede retenido si el hilo
         * se bloquea después en sem_wait(&sem_huecos) cuando el
         * buffer del auditor está lleno. Si no se hiciera esto,
         * todos los hilos que esperan por este nodo quedarían
         * bloqueados innecesariamente hasta que el auditor libere
         * un hueco en su cola. */
        nucleo_liberar_reserva(uid, nodo_objetivo, litros, false);

        /* Paso 4: Reportar al Auditor si es consumo crítico (Productor).
         * Se hace DESPUÉS de liberar el nodo para no retener el candado. */
        if (litros > LITROS_CRITICOS) {
            tls_estado_hilo = "ENCOLANDO";
            
            /* ── Protocolo de negociación del productor ──
             * sem_huecos controla que no se desborde el buffer.
             * sem_mutex protege la escritura en el buffer circular.       */
            sem_wait(&sem_huecos);      /* Esperar espacio en buffer */
            sem_wait(&sem_mutex);       /* Exclusión mutua para buffer */
            
            /* ── Sección crítica: escribir en el buffer circular ── */
            cola_criticos[tail].uid = uid;
            cola_criticos[tail].litros = litros;
            cola_criticos[tail].nodo = nodo_objetivo;
            tail = (tail + 1) % TAM_COLA_AUDITOR;
            
            /* ── Protocolo de liberación del productor ── */
            sem_post(&sem_mutex);       /* Liberar exclusión mutua */
            sem_post(&sem_elementos);   /* Avisar al auditor que hay un item */
        }
    }

    tls_estado_hilo = "TERMINADO";
    return NULL;
}

/* ════════════════════════════════════════════════════════════════════
 * HILO: agente_auditor
 * Consumidor que procesa las alertas de consumo crítico.
 *
 * APAGADO LIMPIO:
 *   El bucle se controla con la bandera 'simulacion_activa'.
 *   Cuando main pone la bandera en false y llama a
 *   agentes_despertar_auditor(), el sem_post desbloquea al auditor
 *   y al revisar la condición del while, termina por sí solo.
 *   Esto evita usar pthread_cancel (que puede provocar deadlocks
 *   si el hilo es cancelado mientras posee un semáforo).
 * ════════════════════════════════════════════════════════════════════ */
void* agente_auditor(void* arg) {
    (void)arg; /* No usamos argumentos */

    log_evento(COL_MAGENTA COL_BOLD, "AUDITOR  | 🟢 Supervisión ACTIVA (Productor-Consumidor)");

    int senales_procesadas = 0;

    /* Bucle controlado por bandera de apagado limpio (sin pthread_cancel) */
    while (simulacion_activa) {
        logger_set_estado_auditor("DURMIENDO");
        tls_estado_hilo = "ESPERANDO";

        /* ── Protocolo de negociación del consumidor ──
         * El auditor se bloquea aquí hasta que un productor deposite
         * un evento en la cola, o hasta que main lo despierte para
         * el apagado con agentes_despertar_auditor().                   */
        sem_wait(&sem_elementos);

        /* Verificar si fue despertado para apagarse */
        if (!simulacion_activa) break;

        /* ── Sección crítica: Extraer del buffer circular ── */
        sem_wait(&sem_mutex);
        EventoCritico ev = cola_criticos[head];
        head = (head + 1) % TAM_COLA_AUDITOR;
        sem_post(&sem_mutex);

        /* ── Protocolo de liberación del consumidor ── */
        sem_post(&sem_huecos); /* Avisar que hay un hueco libre */

        /* Procesamiento de la alerta (fuera de sección crítica) */
        senales_procesadas++;
        char buf_msg[64];
        snprintf(buf_msg, sizeof(buf_msg), "VALIDANDO SENAL #%d", senales_procesadas);
        logger_set_estado_auditor(buf_msg);
        tls_estado_hilo = "VALIDANDO";

        log_evento(COL_MAGENTA, "AUDITOR  | Evaluando sobreconsumo de %.1f L (Usuario #%d)", 
                   ev.litros, ev.uid);
        
        usleep(USLEEP_AUDITOR);  /* Simula tiempo de validacion administrativa (escalado automatico) */
    }

    return NULL;
}
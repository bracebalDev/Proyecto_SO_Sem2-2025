/* Archivo: logger.h
 * Módulo de logging visual para una visualización más amigable de los eventos concurrentes del sistema.
 */
#ifndef LOGGER_H
#define LOGGER_H

/* ── Colores ───────────────────────────────────────────────────── */
#define COL_RESET   "\033[0m"
#define COL_BOLD    "\033[1m"
#define COL_CYAN    "\033[36m"      /* Usuarios: acciones generales      */
#define COL_GREEN   "\033[32m"      /* Reserva exitosa / liberar         */
#define COL_YELLOW  "\033[33m"      /* Espera / consulta / pago          */
#define COL_RED     "\033[31m"      /* Amonestación / cancelación inv.  */
#define COL_MAGENTA "\033[35m"      /* Auditor                           */
#define COL_BLUE    "\033[34m"      /* Telemetría / estado válvulas      */
#define COL_WHITE   "\033[97m"      /* Separadores / mensajes del sistema*/

/* ── API del Logger ─────────────────────────────────────────────────── */

/**
 * Inicializa los semaforos internos del logger.
 * Debe llamarse UNA vez desde main() antes de crear cualquier hilo.
 */
void logger_init(void);

/**
 * Destruye los semaforos del logger.
 * Debe llamarse UNA vez desde main() tras hacer join a todos los hilos.
 */
void logger_close(void);

/**
 * Imprime un mensaje prefijado con el timestamp simulado [HH:MM].
 * Thread-safe: serializado internamente con semaforo.
 *
 * @param color  Macro de color ANSI (p.ej. COL_GREEN). Usa "" para sin color.
 * @param fmt    Formato printf estandar.
 * @param ...    Argumentos de formato.
 */
void log_evento(const char *color, const char *fmt, ...);

/* ── Variables de Estado Transparentes ──────────────────────────────── */
extern __thread const char* tls_estado_hilo;

/**
 * Actualiza el estado del auditor de forma thread-safe (protegido por semaforo).
 * Reemplaza la escritura directa a global_estado_auditor.
 */
void logger_set_estado_auditor(const char *estado);

/**
 * Devuelve el timestamp simulado en segundos transcurridos desde
 * el inicio de la simulación (referencia: tiempo en que logger_init fue llamado).
 * Útil para que el auditor calcule "horas simuladas".
 * 1 hora simulada = DURACION_SIMULACION/12 segundos reales.
 */
double logger_segundos_transcurridos(void);

/** Reinicia el cronómetro interno del logger para iniciar un nuevo día simulado */
void logger_reiniciar_reloj(void);

#endif /* LOGGER_H */

/* Archivo: nucleo_flujo.h */
#ifndef NUCLEO_FLUJO_H
#define NUCLEO_FLUJO_H

#include "eco_config.h"

/* ── Struct de estadísticas exportables ─────────────────────────────── */
typedef struct {
    double total_litros;      /* Litros totales distribuidos             */
    int    amonestaciones;    /* Cancelaciones inválidas penalizadas      */
    int    entregas_criticas; /* Entregas con litros > LITROS_CRITICOS    */
    int    entregas_estandar; /* Entregas con litros <= LITROS_CRITICOS   */
    int total_consultas; /*Total consultas realizadas = Tamaño de solicitudes */
    int total_reservas; /*Total reservas realizadas exitosas; no incluye intentos fallidos */
    double tiempo_espera_total; /* Acumulado de tiempo de espera en reservas */
    double tiempo_espera_max; /* Mayor tiempo de espera registrado en una reserva */
} EstadisticasFlujo;

/* ── Inicialización y destrucción ───────────────────────────────────── */
void nucleo_iniciar_sistema(void);
void nucleo_apagar_sistema(void);

/* ── Primitivas de Sincronización ──────────────────── */

/** Exclusión Mutua — Reservar nodo (ESCRITOR bloqueante).
 *  El hilo se bloquea con sem_wait hasta que el nodo esté libre.
 *  Al retornar true, el candado queda RETENIDO (patrón Escritor estricto).
 *  El candado se libera posteriormente en nucleo_liberar_reserva.         */
bool nucleo_reservar_nodo(int uid, int nodo_idx);

/** Libera un nodo y acumula litros consumidos.
 *  Si fue_cancelacion es true, el hilo adquiere primero el candado
 *  (porque no lo tenía previamente). Si el usuario no es dueño
 *  del nodo, se aplica una sanción (amonestación).                        */
void nucleo_liberar_reserva(int uid, int nodo_idx, double litros, bool fue_cancelacion);

/** Lectores/Escritores: múltiples hilos consultan simultáneamente (read-lock). */
void nucleo_consultar_presion(int uid, int nodo_idx);

/** Aplica una amonestación por cancelación inválida. */
void nucleo_sancionar_usuario(int uid);

/* ── Consultas de estado para telemetría y reporte ──────────────────── */

/** Retorna cuántas válvulas están libres en este instante (read-lock, no bloquea escritura). */
int nucleo_valvulas_libres(void);

/** Copia una snapshot de las estadísticas acumuladas al struct destino. */
void nucleo_get_stats(EstadisticasFlujo *dest);

/** Registra el tiempo de espera y suma una reserva exitosa para la eficiencia */
void nucleo_registrar_eficiencia(double tiempo_esperado);

/** Genera la representación visual rápida*/
void nucleo_get_estado_valvulas_str(char *buf);

#endif /* NUCLEO_FLUJO_H */
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
} EstadisticasFlujo;

/* ── Inicialización y destrucción ───────────────────────────────────── */
void nucleo_iniciar_sistema(void);
void nucleo_apagar_sistema(void);

/* ── Primitivas de Sincronización ──────────────────── */

/** Exclusión Mutua: reserva un nodo libre (write-lock). Retorna true si logró. */
bool nucleo_intentar_reserva(int uid, int nodo_idx);

/** Libera un nodo y acumula litros consumidos. Si el usuario no es dueño conlleva a sanción. */
void nucleo_liberar_reserva(int uid, int nodo_idx, double litros);

/** Lectores/Escritores: múltiples hilos consultan simultáneamente (read-lock). */
void nucleo_consultar_presion(int uid, int nodo_idx);

/** Aplica una amonestación por cancelación inválida. */
void nucleo_sancionar_usuario(void);

/* ── Consultas de estado para telemetría y reporte ──────────────────── */

/** Retorna cuántas válvulas están libres en este instante (read-lock, no bloquea escritura). */
int nucleo_valvulas_libres(void);

/** Copia una snapshot de las estadísticas acumuladas al struct destino. */
void nucleo_get_stats(EstadisticasFlujo *dest);

/** Genera la representación visual rápida*/
void nucleo_get_estado_valvulas_str(char *buf);

#endif /* NUCLEO_FLUJO_H */
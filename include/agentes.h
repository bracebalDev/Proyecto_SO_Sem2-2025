/* Archivo: agentes.h */
#ifndef AGENTES_H
#define AGENTES_H
#include <stdatomic.h>
#include "eco_config.h"

/* ── Variable global de control de apagado limpio ──────────────────────
 * Cuando el main pone esta bandera en 'false', todos los hilos de
 * servicio (auditor, telemetría) terminan su bucle por sí solos,
 * sin necesidad de pthread_cancel (evita riesgo de deadlock).          */
extern _Atomic bool simulacion_activa;;

/* Categoría de Usuario:
 *   RESIDENCIAL → Nodo de Consumo Residencial
 *   INDUSTRIAL  → Nodo de Consumo Industrial
 * Ambos comparten la misma lógica de sincronización pero se
 * diferencian en el log y (en el futuro) en cuotas de litros. */
typedef enum {
    TIPO_RESIDENCIAL = 0,
    TIPO_INDUSTRIAL  = 1
} TipoUsuario;

/* Struct que empaqueta uid + tipo y se pasa como arg al hilo */
typedef struct {
    int        uid;
    TipoUsuario tipo;
} ArgsUsuario;

/* Rutinas de hilos */
void* agente_usuario(void* arg);   /* arg: ArgsUsuario* (memoria gestionada por main) */
void* agente_auditor(void* arg);

/* Gestión de la cola de eventos y estados globales */
void agentes_init(void);
void agentes_close(void);

/* Despierta al auditor si está bloqueado en sem_wait al momento del
 * apagado, para que pueda detectar que simulacion_activa == false.     */
void agentes_despertar_auditor(void);

#endif /* AGENTES_H */
/* Archivo: agentes.h */
#ifndef AGENTES_H
#define AGENTES_H

#include "eco_config.h"

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
void* agente_usuario(void* arg);   /* arg: ArgsUsuario* (heap, el hilo hace free para liberar) */
void* agente_auditor(void* arg);

/* Gestión de la cola de eventos y estados globales */
void agentes_init(void);
void agentes_close(void);

#endif /* AGENTES_H */
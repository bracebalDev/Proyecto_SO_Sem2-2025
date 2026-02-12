/* Archivo: include/agentes.h */
#ifndef AGENTES_H
#define AGENTES_H

#include "eco_config.h"

// Rutinas de hilos
void* agente_usuario(void* arg);
void* agente_auditor(void* arg);

#endif
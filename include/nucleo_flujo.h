/* Archivo: include/nucleo_flujo.h */
#ifndef NUCLEO_FLUJO_H
#define NUCLEO_FLUJO_H

#include "eco_config.h"

// Inicialización y destrucción de recursos
void nucleo_iniciar_sistema();
void nucleo_apagar_sistema();

// Primitivas de Sincronización (API del Kernel)
bool nucleo_intentar_reserva(int uid, int nodo_idx);
void nucleo_liberar_reserva(int uid, int nodo_idx, double litros);
void nucleo_consultar_presion(int uid, int nodo_idx);
void nucleo_sancionar_usuario();

#endif
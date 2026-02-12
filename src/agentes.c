/* Archivo: src/agentes.c */
#include "../include/agentes.h"
#include "../include/nucleo_flujo.h"

// Función auxiliar privada
double _generar_consumo_litros(int tipo) {
    // Industrial (tipo 1): 300 - 800 Litros
    // Residencial (tipo 0): 50 - 550 Litros
    return (tipo == 1) ? (rand() % 500 + 300) : (rand() % 500 + 50);
}

void* agente_usuario(void* arg) {
    ContextoAgente* ctx = (ContextoAgente*)arg;
    int valvula_obj = rand() % NUM_VALVULAS;
    
    // Decisión de acción basada en probabilidad (Enunciado: Reserva es 50%)
    int accion = rand() % 100;

    if (accion < UMBRAL_PROB_RESERVA) {
        // --- CASO 1: SOLICITUD DE RESERVA ---
        
        // Simular tiempo de "pensar" o red
        usleep(rand() % 100000);

        if (nucleo_intentar_reserva(ctx->uid, valvula_obj)) {
            // Éxito: Consumir
            double litros = _generar_consumo_litros(ctx->tipo_usuario);
            
            // Tiempo proporcional al consumo (simulación visual)
            usleep((int)litros * 60); 
            
            nucleo_liberar_reserva(ctx->uid, valvula_obj, litros);
        } else {
            // Fallo: Estaba ocupado. 
            // El usuario espera asignación o se retira.
            // Para la simulación, cuenta como intento fallido/espera.
            pthread_mutex_lock(&g_metricas.mtx_stats);
            g_metricas.tiempo_espera_acumulado++; // Contador simple de fallos
            pthread_mutex_unlock(&g_metricas.mtx_stats);
        }

    } else if (accion < 85) {
        // --- CASO 2: CONSULTA DE PRESIÓN ---
        // Problema Lectores/Escritores
        nucleo_consultar_presion(ctx->uid, valvula_obj);

    } else {
        // --- CASO 3: CANCELACIÓN / PAGO EXCEDENTE ---
        // Regla: "Si solicita cancelación y no posee reservas -> amonestación"
        
        // Intentamos cancelar una válvula al azar donde NO somos dueños
        // (Simulamos el error humano para probar la regla)
        bool soy_dueno = false;
        
        // Verificación rápida (no thread-safe perfecta, pero funcional para la lógica)
        if (g_nodos[valvula_obj].usuario_actual == ctx->uid) {
            soy_dueno = true;
        }

        if (soy_dueno) {
            nucleo_liberar_reserva(ctx->uid, valvula_obj, 0); // Cancelar = 0 litros
        } else {
            nucleo_sancionar_usuario();
        }
    }

    // Limpieza de memoria del hilo
    free(ctx);
    pthread_exit(NULL);
}

void* agente_auditor(void* arg) {
    (void)arg;
    while (g_sistema_activo) {
        // El auditor supervisa en segundo plano
        // Su trabajo real (validar críticos) está inyectado en la función
        // 'nucleo_liberar_reserva' para garantizar integridad de datos.
        // Aquí simulamos su ciclo de vigilancia.
        sleep(1);
    }
    pthread_exit(NULL);
}
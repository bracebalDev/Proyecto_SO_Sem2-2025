/* Archivo: src/telemetria.c */
#include "../include/telemetria.h"

void* agente_telemetria(void* arg) {
    (void)arg;
    
    // Variables locales para cálculo de estadísticas visuales
    double eficiencia = 0.0;
    int total_ops = 0;

    while (g_sistema_activo) {
        // Limpiar consola (ANSI escape codes)
        printf("\033[H\033[J");
        
        printf(ANSI_BOLD ANSI_CYAN "╔════════════════════════════════════════════════════╗\n");
        printf("║      ECO-FLOW 2026: GESTIÓN HÍDRICA INTELIGENTE    ║\n");
        printf("╚════════════════════════════════════════════════════╝" ANSI_RESET "\n\n");

        printf(ANSI_BOLD "ESTADO DE NODOS DE FLUJO (VÁLVULAS):" ANSI_RESET "\n");
        printf("------------------------------------------------------\n");

        for (int i = 0; i < NUM_VALVULAS; i++) {
            // Bloqueamos el mutex visual de cada nodo brevemente para leer estado consistente
            pthread_mutex_lock(&g_nodos[i].mtx_ui);
            
            printf(" Nodo [%02d]: ", g_nodos[i].id_nodo);
            
            if (g_nodos[i].es_critico) {
                // Estado: OCUPADO / ESCRITURA
                printf(ANSI_RED "█ CERRADO (RESERVA) " ANSI_RESET);
                printf("| Usr ID: %03d | Flujo: ACTIVO 🌊", g_nodos[i].usuario_actual);
            } else if (g_nodos[i].lectores_activos > 0) {
                // Estado: LECTURA CONCURRENTE
                printf(ANSI_YELLOW "▒ MONITOREO         " ANSI_RESET);
                printf("| Lectores Activos: %02d         ", g_nodos[i].lectores_activos);
            } else {
                // Estado: LIBRE
                printf(ANSI_GREEN "░ DISPONIBLE        " ANSI_RESET);
                printf("| Esperando solicitud...      ");
            }
            printf("\n");
            
            pthread_mutex_unlock(&g_nodos[i].mtx_ui);
        }
        printf("------------------------------------------------------\n\n");

        // Sección de Estadísticas
        pthread_mutex_lock(&g_metricas.mtx_stats);
        
        total_ops = g_metricas.eficiencia_asignaciones + g_metricas.tiempo_espera_acumulado;
        if (total_ops > 0) {
            eficiencia = ((double)g_metricas.eficiencia_asignaciones / total_ops) * 100.0;
        }

        printf(ANSI_BOLD "REPORTE DEL AUDITOR (EN VIVO):" ANSI_RESET "\n");
        printf(" ► Vol. Total Procesado:  " ANSI_CYAN "%.2f m3" ANSI_RESET "\n", g_metricas.m3_total_procesados);
        printf(" ► Amonestaciones (Err):  " ANSI_RED "%d" ANSI_RESET " (Cancelación inválida)\n", g_metricas.amonestaciones_digitales);
        printf(" ► Validaciones Críticas: " ANSI_YELLOW "%d" ANSI_RESET " (>500L Autorizado)\n", g_metricas.consumos_criticos);
        printf(" ► Operaciones Estándar:  %d\n", g_metricas.consumos_estandar);
        printf(" ► Eficiencia Sistema:    %.1f%% (Nodos Ocupados vs Espera)\n", eficiencia);
        
        pthread_mutex_unlock(&g_metricas.mtx_stats);

        printf("\n[CTRL+C para finalizar simulación]\n");
        usleep(TASA_REFRESCO_UI);
    }
    pthread_exit(NULL);
}
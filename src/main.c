/*
 * =========================================================================================
 * Archivo:    main.c
 * Descripción:
 * Punto de entrada (Entry Point) de la aplicación Eco-Flow.
 * * Responsabilidades:
 * 1. Orquestación: Inicializa y apaga los subsistemas (Logger, Núcleo, Agentes).
 * 2. Gestión de Hilos: Crea y coordina los hilos de Usuarios, Auditor y Telemetría.
 * 3. Interfaz de Usuario: Muestra el menú CLI y el reporte estadístico final.
 * 4. Limpieza: Garantiza que no queden recursos (memoria/semáforos) activos al salir.
 * =========================================================================================
 */

#include "../include/eco_config.h"
#include "../include/nucleo_flujo.h"
#include "../include/agentes.h"
#include "../include/telemetria.h"
#include "../include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>        // Necesario para time(NULL)

/* Limpia el buffer de entrada (stdin) para evitar bucles infinitos en scanf */
static void limpiar_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ── Menú Principal ─────────────────────────────────────────────────── */
static void mostrar_menu(void) {
    printf(COL_WHITE COL_BOLD);
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║     ECO-FLOW 2026 ─ Gestión Hídrica Inteligente ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  1. Caso Fácil   ─    50 solicitudes/día         ║\n");
    printf("║  2. Caso Mediano ─   150 solicitudes/día         ║\n");
    printf("║  3. Caso Base    ─   250 solicitudes/día [Base]  ║\n");
    printf("║  4. Caso Pesado  ─  1000 solicitudes/día         ║\n");
    printf("║  5. Caso Personalizado                           ║\n");
    printf("║  6. Salir                                        ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("  Seleccione una opción: " COL_RESET);
}

/* ── Reporte Final ──────────────────────────────────────────────────── 
 * Recopila las estadísticas acumuladas en el Núcleo y las presenta al usuario.
 * Se llama al finalizar la ejecución de todos los hilos de usuario.
 */

/* (MARÍA LAURA ESTO DEBE MODIFICARSE MÁS ADELANTE PARA LAS ESTADÍSTICAS FINALES QUE SOLICITA EL ENUNCIADO, 
UNA VEZ MODIFICADO QUITA ESTE COMENTARIO JAJAJA) */
static void mostrar_reporte_final(int num_solicitudes) {
    EstadisticasFlujo stats;
    
    // Extraer datos de forma atómica desde el núcleo
    nucleo_get_stats(&stats);

    int total_entregas = stats.entregas_criticas + stats.entregas_estandar;
    double eficiencia  = (total_entregas > 0)
                       ? (100.0 * stats.entregas_estandar / total_entregas)
                       : 0.0;

    printf(COL_WHITE COL_BOLD);
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║         REPORTE FINAL — ECO-FLOW 2026           ║\n");
    printf("║          (coincide con registro del Auditor)    ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf(COL_RESET COL_WHITE);
    printf("  Solicitudes creadas    : %d\n", num_solicitudes);
    printf("  Total litros dist.     : %.1f L\n",   stats.total_litros);
    printf("  Entregas estándar      : %d\n",       stats.entregas_estandar);
    printf("  Entregas críticas      : %d  (> %.0f L)\n",
           stats.entregas_criticas, LITROS_CRITICOS);
    printf("  Amonestaciones         : %d\n",       stats.amonestaciones);
    printf("  Consultas (Lectura)    : %d\n",       stats.total_consultas);
    printf("  Eficiencia estándar    : %.1f%%\n",   eficiencia);
    printf(COL_WHITE COL_BOLD);
    printf("╚══════════════════════════════════════════════════╝\n");
    printf(COL_RESET);
}

/* ════════════════════════════════════════════════════════════════════
 * Función Principal (Main Loop)
 * ════════════════════════════════════════════════════════════════════ */
int main(void) {
    int opcion      = 0;
    int num_solicitudes = 0;

    // Semilla aleatoria inicial para la simulación
    srand((unsigned int)time(NULL));

    do {
        mostrar_menu();

        if (scanf("%d", &opcion) != 1) {
            opcion = 3; /* Fallback automático a Caso Base ante error de entrada */
        }
        limpiar_buffer();

        /* Selección de Escenario */
        switch (opcion) {
            case 1: num_solicitudes = 50;                  break;
            case 2: num_solicitudes = 150;                 break;
            case 3: num_solicitudes = MAX_SOLICITUDES_DIA; break;
            case 4: num_solicitudes = 1000;                break;
            case 5:
                printf("\n  Ingrese el número exacto de solicitudes (N): ");
                if (scanf("%d", &num_solicitudes) != 1) num_solicitudes = MAX_SOLICITUDES_DIA;
                limpiar_buffer();
                break;
            case 6:
                printf("Saliendo...\n");
                return 0;
            default:
                printf("Opción inválida. Iniciando Caso Base.\n");
                num_solicitudes = MAX_SOLICITUDES_DIA;
                break;
        }

        /* ── FASE 1: Inicialización de Subsistemas ── */
        // El Logger debe iniciar primero para capturar eventos de arranque
        logger_init();

        printf(COL_WHITE COL_BOLD
               "\n*** Iniciando simulación: %d solicitudes | Alta contención generada ***\n\n"
               COL_RESET,
               num_solicitudes);

        // Inicializar memoria compartida y semáforos
        nucleo_iniciar_sistema();
        agentes_init();

        /* ── FASE 2: Creación de Hilos de Servicio (Daemon) ── */
        
        // 1. Auditor: Monitorea consumos críticos en segundo plano
        pthread_t auditor;
        pthread_create(&auditor, NULL, agente_auditor, NULL);
        log_evento(COL_MAGENTA, "SISTEMA  | Auditor lanzado   [TID: %lu]", (unsigned long)auditor);

        // 2. Telemetría: Reporta estado de válvulas periódicamente
        pthread_t telemetria;
        pthread_create(&telemetria, NULL, agente_telemetria, NULL);
        log_evento(COL_BLUE, "SISTEMA  | Telemetría lanzada [TID: %lu]", (unsigned long)telemetria);

        /* ── FASE 3: Creación de Hilos de Usuario (Carga de Trabajo) ── */
        // Distribución equitativa: 50% Residencial / 50% Industrial
        pthread_t *usuarios = malloc(sizeof(pthread_t) * (size_t)num_solicitudes);
        if (!usuarios) {
            perror("Error fatal: No hay memoria para hilos de usuarios");
            exit(EXIT_FAILURE);
        }

        int mitad = num_solicitudes / 2;
        for (int i = 0; i < num_solicitudes; i++) {
            ArgsUsuario *args = malloc(sizeof(ArgsUsuario));
            if (!args) { perror("malloc ArgsUsuario"); exit(EXIT_FAILURE); }
            
            args->uid  = i + 1;
            args->tipo = (i < mitad) ? TIPO_RESIDENCIAL : TIPO_INDUSTRIAL;
            
            pthread_create(&usuarios[i], NULL, agente_usuario, args);
        }

        /* ── FASE 4: Barrera de Sincronización (Wait) ── */
        // El hilo main espera a que TODOS los usuarios terminen su ciclo
        for (int i = 0; i < num_solicitudes; i++) {
            pthread_join(usuarios[i], NULL);
        }

        /* ── FASE 5: Cancelación y Limpieza ── */
        // Auditor y Telemetría son bucles infinitos, debemos cancelarlos manualmente
        pthread_cancel(telemetria);
        pthread_join(telemetria, NULL);

        pthread_cancel(auditor);
        pthread_join(auditor, NULL);

        free(usuarios);

        // Destruir semáforos y cerrar recursos
        agentes_close();
        nucleo_apagar_sistema();

        /* ── FASE 6: Reporte Final ── */
        mostrar_reporte_final(num_solicitudes);

        // Cerrar logger (vuelca cualquier buffer pendiente a disco/pantalla)
        logger_close();

    } while (opcion != 6);

    return 0;
}
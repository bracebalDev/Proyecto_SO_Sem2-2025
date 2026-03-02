/* Archivo: main.c */
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

static void limpiar_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ── Menú de inicio ──────────────────────────────────────────────── */
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

/* ── Reporte final 
(MARÍA LAURA ESTO DEBE MODIFICARSE MÁS ADELANTE PARA LAS ESTADÍSTICAS FINALES QUE SOLICITA EL ENUNCIADO, 
UNA VEZ MODIFICADO QUITA ESTE COMENTARIO JAJAJA) */
static void mostrar_reporte_final(int num_solicitudes) {
    EstadisticasFlujo stats;
    nucleo_get_stats(&stats);

    int total_entregas = stats.entregas_criticas + stats.entregas_estandar;
    double eficiencia  = (total_entregas > 0)
                       ? (100.0 * stats.entregas_estandar / total_entregas)
                       : 0.0;

    /* Las métricas aquí son IDÉNTICAS a las que imprimió el auditor al final */
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
    printf("  Eficiencia estándar    : %.1f%%\n",   eficiencia);
    printf(COL_WHITE COL_BOLD);
    printf("╚══════════════════════════════════════════════════╝\n");
    printf(COL_RESET);
}

/* Configuración inicial del menú para ejecución rápida*/
/* ════════════════════════════════════════════════════════════════════ */
int main(void) {
    int opcion        = 0;
    int num_solicitudes = 0;

    srand((unsigned int)time(NULL));

    do {
        mostrar_menu();

        if (scanf("%d", &opcion) != 1) {
            opcion = 3; /* Fallback al Caso Base */
        }
        limpiar_buffer();

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

        /* ── Inicializar el logger ANTES que cualquier hilo ── */
        logger_init();

        printf(COL_WHITE COL_BOLD
               "\n*** Iniciando simulación: %d solicitudes | Alta contención generada ***\n\n"
               COL_RESET,
               num_solicitudes);

        /* ── Inicializar el núcleo y los agentes ── */
        nucleo_iniciar_sistema();
        agentes_init();

    /* ── Crear el Auditor ── */
    pthread_t auditor;
    pthread_create(&auditor, NULL, agente_auditor, NULL);
    log_evento(COL_MAGENTA,
               "SISTEMA  | Auditor lanzado   [TID: %lu]",
               (unsigned long)auditor);

    /* ── Crear hilo de Telemetría ── */
    pthread_t telemetria;
    pthread_create(&telemetria, NULL, agente_telemetria, NULL);
    log_evento(COL_BLUE,
               "SISTEMA  | Telemetría lanzada [TID: %lu]",
               (unsigned long)telemetria);

    /* ── Crear hilos de Usuarios ──
     * Enunciado: requiere Nodo de Consumo Residencial y Nodo de Consumo Industrial.
     * Distribución: primera mitad = RESIDENCIAL, segunda mitad = INDUSTRIAL.
     */
    pthread_t *usuarios = malloc(sizeof(pthread_t) * (size_t)num_solicitudes);
    if (!usuarios) {
        perror("Error de memoria para hilos de usuarios");
        exit(EXIT_FAILURE);
    }

    int mitad = num_solicitudes / 2;
    for (int i = 0; i < num_solicitudes; i++) {
        /* Cada ArgsUsuario se aloja en el heap; el hilo hace free() al inicio */
        ArgsUsuario *args = malloc(sizeof(ArgsUsuario));
        if (!args) { perror("malloc ArgsUsuario"); exit(EXIT_FAILURE); }
        args->uid  = i + 1;
        args->tipo = (i < mitad) ? TIPO_RESIDENCIAL : TIPO_INDUSTRIAL;
        pthread_create(&usuarios[i], NULL, agente_usuario, args);
    }

    /* ── Esperar a que todos los usuarios terminen ── */
    for (int i = 0; i < num_solicitudes; i++) {
        pthread_join(usuarios[i], NULL);
    }

    /* ── Cancelar servicios de fondo ── */
    pthread_cancel(telemetria);
    pthread_join(telemetria, NULL);

    pthread_cancel(auditor);
    pthread_join(auditor, NULL);

    free(usuarios);

    /* ── Apagar núcleo y colas ── */
    agentes_close();
    nucleo_apagar_sistema();

    /* ── Reporte final (mismas métricas que el auditor) ── */
    mostrar_reporte_final(num_solicitudes);

    logger_close();

    } while (opcion != 6);

    return 0;
}

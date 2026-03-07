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
#include <stdatomic.h>
#include <time.h>
#ifdef __linux__
#include <sys/resource.h> /* Necesario para getrusage (reporte de Hardware) */
#endif

bool modo_debug = false; // Por defecto desactivado

/* ── Variable global de control de apagado limpio ──────────────────────
 * Cuando se pone en false, los hilos de servicio (auditor, telemetría)
 * terminan sus bucles por sí solos, sin necesidad de pthread_cancel.*/
_Atomic bool simulacion_activa = true;

/* Limpia el buffer de entrada (stdin) para evitar bucles infinitos en scanf */
static void limpiar_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* ── Menú Principal ─────────────────────────────────────────────────── */
static void mostrar_menu(void) {
    printf(COL_WHITE COL_BOLD);
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║     ECO-FLOW 2026 ─ Gestión Hídrica Inteligente  ║\n");
    printf("╠══════════════════════════════════════════════════╣\n");
    printf("║  0. Ver Monitoreo en Tiempo Real:   [%s]         ║\n", modo_debug ? "SI" : "NO");
    printf("║  1. Caso Fácil   ─    50 solicitudes/día         ║\n");
    printf("║  2. Caso Mediano ─   150 solicitudes/día         ║\n");
    printf("║  3. Caso Base    ─   250 solicitudes/día [Base]  ║\n");
    printf("║  4. Caso Pesado  ─  1000 solicitudes/día         ║\n");
    printf("║  5. Caso Personalizado                           ║\n");
    printf("║  6. Salir                                        ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("  Seleccione una opción: " COL_RESET);
}

/* ── Reporte de Recursos de Hardware ───────────────────────────────── */
static void mostrar_recursos_sistema(void) {
#ifdef __linux__
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        printf(COL_CYAN "\n=======================================================\n");
        printf("💻 REPORTE DE CONSUMO DE RECURSOS DEL SISTEMA (Hardware)\n");
        printf("=======================================================\n" COL_RESET);
        printf("  - Tiempo de CPU (Usuario): %ld.%06ld seg\n", usage.ru_utime.tv_sec, (long)usage.ru_utime.tv_usec);
        printf("  - Tiempo de CPU (Sistema): %ld.%06ld seg\n", usage.ru_stime.tv_sec, (long)usage.ru_stime.tv_usec);
        printf("  - Memoria RAM máxima usada (RSS): %ld KB\n", usage.ru_maxrss);
        printf(COL_CYAN "=======================================================\n\n" COL_RESET);
    }
#else
    /* Lo que se imprime si lo corres en Windows */
    printf(COL_CYAN "\n=======================================================\n");
    printf("💻 REPORTE DE CONSUMO DE RECURSOS (No disponible en Windows)\n");
    printf("=======================================================\n\n" COL_RESET);
#endif
}

/* ── Reporte Final ──────────────────────────────────────────────────── 
 * Recopila las estadísticas acumuladas en el Núcleo y las presenta al usuario.
 * Se llama al finalizar la ejecución de todos los hilos de usuario.
 */

static void mostrar_reporte_final(int total_solicitudes_mes) {
    EstadisticasFlujo stats;
    nucleo_get_stats(&stats);

    /* Cálculos de conversión y eficiencia */
    double metros_cubicos = stats.total_litros / 1000.0;

    int total_entregas = stats.entregas_criticas + stats.entregas_estandar;
    double eficiencia_estandar = (total_entregas > 0)
                       ? (100.0 * stats.entregas_estandar / total_entregas)
                       : 0.0;

    double promedio_espera = 0.0;
    if (stats.total_reservas > 0) {
        promedio_espera = stats.tiempo_espera_total / stats.total_reservas;
    }

    printf(COL_WHITE COL_BOLD);
    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║     📊 REPORTE MENSUAL (30 DÍAS) — ECO-FLOW 2026       ║\n");
    printf("║          (coincide con registro del Auditor)           ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf(COL_RESET COL_WHITE);

    printf("💧 Total de agua procesada:       %.2f m3 \n", metros_cubicos);
    printf(COL_RED "🚨 Amonestaciones digitales:      %d errores de cancelación\n" COL_RESET, stats.amonestaciones);
    printf("⚠️  Señales de consumo Crítico:   %d entregas (> %.0f L)\n", stats.entregas_criticas, LITROS_CRITICOS);
    printf("✅ Señales de consumo Estándar:  %d entregas (%.1f%% del total)\n", stats.entregas_estandar, eficiencia_estandar);
    
    printf(" 🔍 Consultas de presion (Solo lectura):  %d transacciones\n", stats.total_consultas);

    printf(COL_CYAN "\n⚙️  EFICIENCIA DEL SISTEMA (Nodos vs Espera):\n");
    printf("   - Nodos ocupados (Reservas):   %d\n", stats.total_reservas);
    printf("   - Tiempo de espera global:     %.2f segundos\n", stats.tiempo_espera_total);
    printf("   - Tiempo de espera promedio:   %.2f segundos por reserva\n" , promedio_espera);
    printf("   - Tiempo MAXIMO de espera (Peor caso): %.2f segundos\n" COL_RESET, stats.tiempo_espera_max);

    printf(COL_WHITE "\n=======================================================\n");
    printf("Total de solicitudes procesadas en el mes: %d\n", total_solicitudes_mes);
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf(COL_RESET);

    mostrar_recursos_sistema();
}

/* ════════════════════════════════════════════════════════════════════
 * Función Principal (Main Loop)
 * ════════════════════════════════════════════════════════════════════ */
int main(void) {
    int opcion      = 0;
    int num_solicitudes = 0;

    /* Semilla aleatoria inicial para la simulación */
    srand((unsigned int)time(NULL));

    do {
        mostrar_menu();

        if (scanf("%d", &opcion) != 1) {
            opcion = 3; /* Fallback automático a Caso Base ante error de entrada */
        }
        limpiar_buffer();

        /* Selección de Escenario */
        switch (opcion) {
            case 0: // Opción oculta de Debugger
                 modo_debug = !modo_debug; // Si era true pasa a false, y viceversa
               printf("\n[SISTEMA] Monitoreo Detallado: %s\n", modo_debug ? "ACTIVADO" : "DESACTIVADO");
                continue; // Vuelve al menú para mostrar el cambio
            case 1: num_solicitudes = 50;                  break;
            case 2: num_solicitudes = 150;                 break;
            case 3: num_solicitudes = MAX_SOLICITUDES_DIA; break;
            case 4: num_solicitudes = 1000;                break;
            case 5:
                printf("\n  Ingrese el número exacto de solicitudes (N): ");
    
                // Si la lectura falla (el usuario coloca una letra o símbolo), se muestra un mensaje de error y se limpia el buffer para evitar un bucle infinito.
                if (scanf("%d", &num_solicitudes) != 1) { 
                    printf(COL_RED "\n[ERROR DE VALIDACIÓN]  no es un número válido.\n" COL_RESET);
                    while (getchar() != '\n'); 
                    printf("Volviendo al menú principal...\n");
    
                    continue; 
                }
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
        /* El Logger debe iniciar primero para capturar eventos de arranque */
        logger_init();

        /* Activar la bandera de simulación para los hilos de servicio */
        simulacion_activa = true;

        printf(COL_WHITE COL_BOLD
               "\n*** Iniciando simulación: %d solicitudes | Alta contención generada ***\n\n"
               COL_RESET,
               num_solicitudes);

        /* Inicializar memoria compartida y semáforos */
        nucleo_iniciar_sistema();
        agentes_init();

        /* ── FASE 2: Creación de Hilos de Servicio (Daemon) ── */
        
        /* 1. Auditor: Monitorea consumos críticos en segundo plano */
        pthread_t auditor;
        if (pthread_create(&auditor, NULL, agente_auditor, NULL) != 0) {
            perror("Error fatal: No se pudo crear el hilo Auditor");
            exit(EXIT_FAILURE);
        }
        // --- CONTROL DE LOGS ---
        if (modo_debug) {
            log_evento(COL_MAGENTA, "SISTEMA  | Auditor lanzado   [TID: %lu]", (unsigned long)auditor);
        }

        /* 2. Telemetría: Reporta estado de válvulas periódicamente */
        pthread_t telemetria;
        if (pthread_create(&telemetria, NULL, agente_telemetria, NULL) != 0) {
            perror("Error fatal: No se pudo crear el hilo Telemetría");
            exit(EXIT_FAILURE);
        }
        // --- CONTROL DE LOGS ---
        if (modo_debug) {
            log_evento(COL_BLUE, "SISTEMA  | Telemetría lanzada [TID: %lu]", (unsigned long)telemetria);
        }

        /* ── FASE 3: Creación de Hilos de Usuario (Carga de Trabajo) ── */
        /* Variable para acumular cuánta gente entró realmente en todo el mes */
        int total_solicitudes_mes = 0;

        printf(COL_CYAN "\n=======================================================\n");
        printf("📅 INICIANDO SIMULACIÓN DE 1 MES (30 Días Estocásticos)\n");
        printf("   Capacidad Máxima Diaria: %d solicitudes\n", num_solicitudes);
        printf("=======================================================\n" COL_RESET);

        /* ── Simulación de 30 Días ── */
        
        for (int dia = 1; dia <= 30; dia++) {

            /* ── Reiniciar el reloj para que el Logger vuelva a las 06:00 ── */
            logger_reiniciar_reloj();

            /* el if de modo_debug se pone aquí para que imprima el mensaje de inicio de día solo una vez por
            día, no por cada solicitud. Así se evita saturar la consola con mensajes repetitivos en modo
            debug, pero aún se tiene un indicador claro del avance diario.*/ 
            
            if (modo_debug) {
            printf(COL_CYAN "\n🌅 --- INICIANDO DÍA %d DE 30 --- 🌅\n" COL_RESET, dia);
            }

            /* Variabilidad Diaria: El tráfico varía entre el 60% y el 100% del tope elegido */
            int min_solicitudes = (int)(num_solicitudes * 0.60);
            int solicitudes_hoy = min_solicitudes + (rand() % (num_solicitudes - min_solicitudes + 1));
            
            total_solicitudes_mes += solicitudes_hoy; /* Acumulación para reporte final */

            /* ── FASE 3: Creación de Hilos de Usuario (Carga del Día) ──
             *
             * CORRECCIÓN DE FUGA DE MEMORIA:
             * Antes se hacía un malloc individual por cada ArgsUsuario y el
             * hilo hacía free(params) al inicio. Esto causaba fugas si el
             * hilo era cancelado antes de ejecutar el free.
             *
             * Ahora se asigna UN SOLO bloque de memoria (args_array) para
             * todos los argumentos del día, y se libera DESPUÉS del join.
             * Esto garantiza que la memoria siempre se libere, sin importar
             * cómo termine el hilo.                                         */
            pthread_t *usuarios = malloc(sizeof(pthread_t) * (size_t)solicitudes_hoy);
            ArgsUsuario *args_array = malloc(sizeof(ArgsUsuario) * (size_t)solicitudes_hoy);
            if (!usuarios || !args_array) {
                perror("Error fatal: No hay memoria para hilos de usuarios");
                if (usuarios) free(usuarios);
                if (args_array) free(args_array);
                exit(EXIT_FAILURE);
            }
            /* Crear atributos del hilo para reducir el consumo de RAM a 64KB */
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setstacksize(&attr, 64 * 1024);

            int mitad = solicitudes_hoy / 2;
            bool limite_alcanzado = false; /* Bandera de control estructurado */
            for (int i = 0; i < solicitudes_hoy && !limite_alcanzado; i++) {
                /* Asignar datos directamente al arreglo (sin malloc individual) */
                args_array[i].uid  = (dia * 1000) + (i + 1); 
                args_array[i].tipo = (i < mitad) ? TIPO_RESIDENCIAL : TIPO_INDUSTRIAL;
                
                /* Se pasa la DIRECCIÓN dentro del arreglo, no un puntero heap individual */
                if (pthread_create(&usuarios[i], &attr, agente_usuario, &args_array[i]) != 0) {
                    perror("Advertencia: Límite de hilos del sistema alcanzado");
                    solicitudes_hoy = i;
                    limite_alcanzado = true;
                }
            }
            /* Se destruyen los atributos de los hilos una vez usados para evitar fugas */
            pthread_attr_destroy(&attr);
            /* ── FASE 4: Barrera de Sincronización Diaria (Wait) ──
             * El sistema espera a que la planta cierre operaciones por el día de hoy */
            for (int i = 0; i < solicitudes_hoy; i++) {
                pthread_join(usuarios[i], NULL);
            }

            /* Limpieza de la memoria del día antes de arrancar el siguiente.
             * Tanto el arreglo de hilos como el de argumentos se liberan aquí
             * porque los hilos ya terminaron (join completado).              */
            free(usuarios);
            free(args_array);
        }
     
        //usleep(500000); /* Pausa para que el auditor procese las últimas señales */
        
        // Si el monitoreo está apagado, esperamos casi nada (0.01s). Si está encendido, esperamos 0.5s.
        usleep(modo_debug ? 500000 : 10000);
        /* ── FASE 5: Apagado Limpio (sin pthread_cancel) ──
         *
         * PROTOCOLO DE APAGADO:
         * 1. Se baja la bandera simulacion_activa para que los hilos de
         *    servicio (auditor, telemetría) salgan de sus bucles.
         * 2. Se despierta al auditor con agentes_despertar_auditor()
         *    por si está bloqueado en sem_wait(&sem_elementos).
         * 3. Se espera con pthread_join a que ambos terminen limpiamente.
         *
         * Esto reemplaza el uso de pthread_cancel, que podía causar
         * deadlocks si cancelaba un hilo mientras poseía un semáforo.   */
        simulacion_activa = false;

        /* Despertar al auditor si está bloqueado esperando eventos */
        agentes_despertar_auditor();

        /* Esperar a que los hilos de servicio terminen por sí solos */
        pthread_join(telemetria, NULL);
        pthread_join(auditor, NULL);

        printf(COL_GREEN "\n✅ Mes simulado con éxito. Total transacciones intentadas: %d\n" COL_RESET, total_solicitudes_mes);

        /* Destruir semáforos y cerrar recursos */
        agentes_close();
        nucleo_apagar_sistema();

        /* ── FASE 6: Reporte Final ── */
        mostrar_reporte_final(total_solicitudes_mes);

        /* Cerrar logger (vuelca cualquier buffer pendiente a disco/pantalla) */
        logger_close();

    } while (opcion != 6);

    return 0;
}
/* Archivo: src/main.c */
/*
 * PROYECTO: Eco-Flow 2026
 * ASIGNATURA: Sistemas Operativos
 * DESCRIPCIÓN: Simulación de concurrencia con Hilos POSIX y Semáforos.
 */

#include "../include/eco_config.h"
#include "../include/nucleo_flujo.h"
#include "../include/agentes.h"
#include "../include/telemetria.h"

int main() {
    // 1. Configuración Inicial
    srand(time(NULL));
    nucleo_iniciar_sistema();

    printf("Iniciando Eco-Flow 2026...\n");

    // 2. Levantar Hilos de Soporte (UI y Auditor)
    pthread_t t_ui, t_auditor;
    
    if (pthread_create(&t_ui, NULL, agente_telemetria, NULL) != 0) {
        perror("Error fatal iniciando Telemetría");
        return 1;
    }
    if (pthread_create(&t_auditor, NULL, agente_auditor, NULL) != 0) {
        perror("Error fatal iniciando Auditor");
        return 1;
    }

    // 3. Ciclo Principal de Simulación (Generador de Carga)
    // Simula el paso del tiempo de 06:00 a 18:00
    time_t tiempo_inicio = time(NULL);
    int contador_usuarios = 0;

    while (difftime(time(NULL), tiempo_inicio) < DURACION_SIMULACION) {
        
        // Generar ráfagas aleatorias de usuarios
        int nuevos_usuarios = (rand() % 4) + 1; // 1 a 4 usuarios por ciclo

        for (int i = 0; i < nuevos_usuarios; i++) {
            if (contador_usuarios >= MAX_SOLICITUDES_DIA) break;

            ContextoAgente *ctx = malloc(sizeof(ContextoAgente));
            if (ctx == NULL) {
                perror("Memoria insuficiente para agente");
                continue;
            }

            ctx->uid = contador_usuarios++;
            ctx->tipo_usuario = rand() % 2; // 0: Residencial, 1: Industrial

            pthread_t t_usr;
            pthread_attr_t attr;
            
            // CRÍTICO: Usar hilos DETACHED para liberar memoria automáticamente al terminar.
            // Si no hacemos esto, el sistema operativo se satura esperando joins.
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

            if (pthread_create(&t_usr, &attr, agente_usuario, ctx) != 0) {
                free(ctx); // Evitar leak si falla creación
            }
            
            pthread_attr_destroy(&attr);
        }

        // Control de ritmo (simula las horas pasando)
        usleep((rand() % 300000) + 100000); // 100ms - 400ms
    }

    // 4. Finalización Controlada
    g_sistema_activo = false;
    
    // Esperar a los hilos de infraestructura
    pthread_join(t_ui, NULL);
    pthread_join(t_auditor, NULL);

    printf("\n" ANSI_BOLD "=== INFORME FINAL (18:00) ===" ANSI_RESET "\n");
    printf("Limpiando recursos del sistema...\n");
    
    nucleo_apagar_sistema();
    
    return 0;
}
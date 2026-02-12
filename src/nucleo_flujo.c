/* Archivo: src/nucleo_flujo.c */
#include "../include/nucleo_flujo.h"

// Instanciación de variables globales
NodoFlujo g_nodos[NUM_VALVULAS];
MetricasEco g_metricas;
volatile bool g_sistema_activo = true;

void nucleo_iniciar_sistema() {
    // 1. Resetear métricas
    g_metricas.m3_total_procesados = 0;
    g_metricas.amonestaciones_digitales = 0;
    g_metricas.consumos_criticos = 0;
    g_metricas.consumos_estandar = 0;
    g_metricas.eficiencia_asignaciones = 0;
    g_metricas.tiempo_espera_acumulado = 0;
    pthread_mutex_init(&g_metricas.mtx_stats, NULL);

    // 2. Inicializar Válvulas y Semáforos
    for (int i = 0; i < NUM_VALVULAS; i++) {
        g_nodos[i].id_nodo = i;
        g_nodos[i].usuario_actual = -1;
        g_nodos[i].es_critico = false;
        g_nodos[i].lectores_activos = 0;

        // Semáforo binario en 1: Permite 1 escritor a la vez
        sem_init(&g_nodos[i].sem_exclusion, 0, 1);
        
        // Semáforo binario en 1: Protege la variable de conteo de lectores
        sem_init(&g_nodos[i].sem_lectores, 0, 1);
        
        // Mutex visual
        pthread_mutex_init(&g_nodos[i].mtx_ui, NULL);
    }
}

void nucleo_apagar_sistema() {
    pthread_mutex_destroy(&g_metricas.mtx_stats);
    for (int i = 0; i < NUM_VALVULAS; i++) {
        sem_destroy(&g_nodos[i].sem_exclusion);
        sem_destroy(&g_nodos[i].sem_lectores);
        pthread_mutex_destroy(&g_nodos[i].mtx_ui);
    }
}

// --- Lógica: Exclusión Mutua (Escritores) ---
bool nucleo_intentar_reserva(int uid, int nodo_idx) {
    NodoFlujo *nodo = &g_nodos[nodo_idx];

    // Usamos trywait para no bloquear el hilo indefinidamente si está ocupado.
    // Esto simula que el sistema verifica disponibilidad inmediatamente.
    if (sem_trywait(&nodo->sem_exclusion) == 0) {
        // Entrando a Sección Crítica (Escritura)
        
        pthread_mutex_lock(&nodo->mtx_ui);
        nodo->es_critico = true;
        nodo->usuario_actual = uid;
        pthread_mutex_unlock(&nodo->mtx_ui);

        // Simulamos tiempo de validación de sistema (bloque horario)
        usleep(50000); 
        
        return true; // Reserva exitosa
    }
    return false; // Nodo ocupado
}

void nucleo_liberar_reserva(int uid, int nodo_idx, double litros) {
    NodoFlujo *nodo = &g_nodos[nodo_idx];

    // Actualizar Estado Visual
    pthread_mutex_lock(&nodo->mtx_ui);
    nodo->es_critico = false;
    nodo->usuario_actual = -1;
    pthread_mutex_unlock(&nodo->mtx_ui);

    // Actualizar Estadísticas (Monitor)
    pthread_mutex_lock(&g_metricas.mtx_stats);
    g_metricas.m3_total_procesados += (litros / 1000.0);
    g_metricas.eficiencia_asignaciones++;
    
    // Regla de Negocio 4: Verificación de Auditor
    if (litros > LITROS_CRITICOS) {
        g_metricas.consumos_criticos++; 
        // Nota: La validación del auditor es implícita en el contador
    } else {
        g_metricas.consumos_estandar++;
    }
    pthread_mutex_unlock(&g_metricas.mtx_stats);

    // Salir Sección Crítica: Liberar semáforo
    sem_post(&nodo->sem_exclusion);
}

// --- Lógica: Lectores / Escritores (Lectores) ---
void nucleo_consultar_presion(int uid, int nodo_idx) {
    (void)uid; // Unused
    NodoFlujo *nodo = &g_nodos[nodo_idx];

    // 1. Protocolo de Entrada de Lectores
    sem_wait(&nodo->sem_lectores);
    nodo->lectores_activos++;
    if (nodo->lectores_activos == 1) {
        // El primer lector cierra la puerta a los escritores
        sem_wait(&nodo->sem_exclusion);
    }
    sem_post(&nodo->sem_lectores);

    // 2. Sección Crítica de Lectura
    // Varios hilos pueden estar aquí simultáneamente
    usleep(rand() % 40000 + 20000); // Leer sensor (20-60ms)

    // 3. Protocolo de Salida de Lectores
    sem_wait(&nodo->sem_lectores);
    nodo->lectores_activos--;
    if (nodo->lectores_activos == 0) {
        // El último lector abre la puerta a los escritores
        sem_post(&nodo->sem_exclusion);
    }
    sem_post(&nodo->sem_lectores);
}

void nucleo_sancionar_usuario() {
    pthread_mutex_lock(&g_metricas.mtx_stats);
    g_metricas.amonestaciones_digitales++;
    pthread_mutex_unlock(&g_metricas.mtx_stats);
}
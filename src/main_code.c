#include "definiciones.h"

int main() {
    int h, n, i; // variables para iterar horas, nodos, solicitudes
    int id_memoria;
    MemoriaSistema *sistema;
    pid_t pid;
    
   
    //  sizeof reserva el tamaño exacto de nuestra estructura
    id_memoria = shmget(IPC_PRIVATE, sizeof(MemoriaSistema), IPC_CREAT | 0666);
    if (id_memoria == -1) {
        perror("Error en shmget");
        exit(1);
    }

    
    sistema = (MemoriaSistema *)shmat(id_memoria, NULL, 0);
    
    // inicializamoslos datos 
    sistema->total_litros_consumidos = 0;
    sistema->contador_amonestaciones = 0;
    // inicializamos la matriz de reservas a 0 (libre)
    for( h=0; h<HORAS_SIMULACION; h++) {
        for( n=0; n<NUM_NODOS; n++) {
            sistema->matriz_reservas[h][n] = 0; 
        }
    }

    printf("Sistema Eco-Flow 2026 iniciado. Creando 250 solicitudes...\n");

    // crear los 250 procesos hijos
    for ( i = 0; i < TOTAL_SOLICITUDES; i++) {
         pid = fork();
        
        if (pid < 0) {
            perror("Error al crear proceso");
            break;
        } 
        else if (pid == 0) {
            // --- ESTO LO HACE EL HIJO ---
            // Pasamos el ID del hijo y el puntero a la memoria compartida
            ejecutar_usuario(i + 1, sistema, 0); 
            exit(0); // El hijo muere aquí para no crear más hijos
        }
    }

    //El Padre espera a que todos terminen
    for ( i = 0; i < TOTAL_SOLICITUDES; i++) {
        wait(NULL);
    }

    //  Mostrar resultados y limpiar
    printf("\n--- SIMULACIÓN FINALIZADA ---\n");
    printf("Total litros: % d\n", sistema->total_litros_consumidos);
    
    shmdt(sistema); // Desconectar
    shmctl(id_memoria, IPC_RMID, NULL); // Borrar la memoria del sistema

    return 0;
}

// FUNCIONES TEMPORARES PARA VER SI ESTO FUNCIONA O.O 
void ejecutar_usuario(int id, MemoriaSistema *sistema, int sem_id) {
    printf("[Hijo %d] ¡Hola! Soy un proceso de Eco-Flow y veo la memoria.\n", id);
}

void ejecutar_auditor(MemoriaSistema *sistema, int sem_id) {
    printf("[Auditor] Supervisando el sistema...\n");
}
//////////////////////////////
#ifndef DEFINICIONES_H
#define DEFINICIONES_H
#include <stdatomic.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include "../include/eco_config.h" 

/* Variable Thread-Local para el logger */
extern __thread const char *tls_estado_hilo; 

/* [Estructura Privada: NodoFlujo]
 * Se mantiene aquí para que nucleo_flujo.c la vea.
 */
typedef struct {
    _Atomic(bool) ocupado;
    int  propietario_uid;

    /* Semaforos del patron Lectores-Escritores */
    sem_t sem_escritura;
    sem_t sem_lectores;
    int   cont_lectores;

    /* Semaforo de turno (fairness): evita starvation de escritores.
     * Un escritor que llega hace sem_wait(sem_turno), bloqueando a
     * nuevos lectores hasta que el escritor entre a su seccion critica. */
    sem_t sem_turno;
} NodoFlujo;

#endif
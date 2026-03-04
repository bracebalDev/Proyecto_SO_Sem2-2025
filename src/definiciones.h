#ifndef DEFINICIONES_H
#define DEFINICIONES_H

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include "../include/eco_config.h" 

/* Variable Thread-Local para el logger */
extern __thread const char *tls_estado_hilo; 

/* [Estructura Privada: NodoFlujo]
 * Se mantiene aquí para que nucleo_flujo.c la vea, pero NO debe estar 
 * redefinida en el .c.
 */
typedef struct {
    bool ocupado;
    int  propietario_uid;
    
    /* Semáforos del patrón Lectores-Escritores */
    sem_t sem_escritura;   
    sem_t sem_lectores;    
    int   cont_lectores;    
} NodoFlujo;

#endif
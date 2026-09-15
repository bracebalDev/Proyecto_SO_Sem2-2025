# 💧 Eco-Flow 2026 — Gestión Hídrica Inteligente

> Simulador concurrente de distribución de agua basado en la **Ley de Preservación de Recursos Hídricos de 2026**. Proyecto de la asignatura **Sistemas Operativos** — Problema de Sincronización.

---

## 👥 Autores

| Nombre |
|---|---|
| Derginette Blanco |
| María Laura Gutiérrez |
| Brayan Ceballos |
| Andrés Crespo |

---

## 📋 Descripción del Problema

El **Centro de Control Hídrico** opera con **10 válvulas principales** (nodos de flujo) que pueden ser reservadas por lapsos de 1 hora de consumo intenso. El sistema opera entre las **06:00 y las 18:00**, procesando hasta **250 solicitudes de servicio** por día en el caso base.

Un **Auditor de Flujo** supervisa el sistema, validando consumos que superan los 500 litros (señales críticas) para evitar multas automáticas. La simulación reproduce **30 días** de operación con variabilidad estocástica.

### Reglas de Sincronización Implementadas

| Regla | Mecanismo | Descripción |
|---|---|---|
| **R1** Exclusión Mutua | Semáforo binario (`sem_escritura`) | Reserva y cancelación bloquean acceso exclusivo al nodo |
| **R2** Lectores/Escritores | Patrón L/E con fairness (`sem_turno`) | Múltiples consultas simultáneas sin bloquear; escritores no sufren inanición |
| **R3** Validación | Verificación de propiedad | Amonestación digital por cancelación sin reserva activa |
| **R4** Consumo Crítico | Cola Productor-Consumidor | Auditor valida consumos > 500L vía buffer circular con semáforos |

---

## 🏗️ Arquitectura

```
Eco-Flow 2026/
├── include/                    # Cabeceras públicas
│   ├── eco_config.h            # Parámetros globales y escalado temporal
│   ├── definiciones.h          # Estructura NodoFlujo (semáforos L/E)
│   ├── nucleo_flujo.h          # API del motor de concurrencia
│   ├── agentes.h               # Hilos de usuario y auditor
│   ├── logger.h                # API de logging thread-safe
│   └── telemetria.h            # Hilo de monitoreo en tiempo real
├── src/                        # Implementaciones
│   ├── main.c                  # Orquestación, menú CLI, ciclo de 30 días
│   ├── nucleo_flujo.c          # Motor de sincronización (L/E + semáforos)
│   ├── agentes.c               # Lógica de hilos usuarios + auditor P/C
│   ├── logger.c                # Logging con timestamps simulados y TLS
│   ├── telemetria.c            # Dashboard de válvulas en tiempo real
│   └── ACTUALIZACIONES.md      # Changelog de correcciones
├── docs/                       # Documentación
│   ├── ProyectoSincronizacion2-2025.pdf  # Enunciado original
│   └── Documentación.pdf       # Documentación del proyecto
├── config/                     # Configuración
├── makefile                    # Sistema de compilación
└── .gitignore
```

### Diagrama de Procesos Concurrentes

```
┌─────────────────┐
│    main.c        │  Orquestador
│  (Hilo Principal)│
└────────┬────────┘
         │ Crea hilos
         ├──────────────────────────────────┐
         │                                  │
    ┌────▼─────┐                      ┌─────▼──────┐
    │ Agente    │ ×N por día          │ Agente     │  Daemon
    │ Usuario   │ (Residencial/       │ Auditor    │  (Productor-
    │           │  Industrial)        │            │   Consumidor)
    └────┬─────┘                      └─────▲──────┘
         │ sem_escritura                    │ sem_elementos
         │ sem_lectores                     │ (Cola circular)
         │ sem_turno                        │
    ┌────▼──────────────────────────────────┤
    │        nucleo_flujo.c                 │
    │   (10 Nodos con semáforos L/E)        │
    └───────────────────────────────────────┘
         │
    ┌────▼─────┐
    │ Agente    │  Daemon
    │Telemetría │  (Monitoreo)
    └──────────┘
```

---

## ⚙️ Requisitos

- **Compilador**: GCC con soporte para C11 y pthreads
- **Sistema Operativo**: Linux (recomendado) o Windows con MinGW/MSYS2
- **Librerías**: POSIX Threads (`-pthread`), Semáforos POSIX (`semaphore.h`)

---

## 🚀 Compilación y Ejecución

### Compilar el proyecto

```bash
make
```

### Compilar con símbolos de depuración

```bash
make DEBUG=1
```

### Compilar y ejecutar

```bash
make run
```

### Limpiar archivos generados

```bash
make clean
```

---

## 📖 Uso del Menú

Al ejecutar el programa, se presenta un menú interactivo:

```
╔══════════════════════════════════════════════════╗
║     ECO-FLOW 2026 ─ Gestión Hídrica Inteligente  ║
╠══════════════════════════════════════════════════╣
║  0. Ver Monitoreo en Tiempo Real:   [SI]         ║
║  1. Caso Fácil   ─    50 solicitudes/día         ║
║  2. Caso Mediano ─   150 solicitudes/día         ║
║  3. Caso Base    ─   250 solicitudes/día [Base]  ║
║  4. Caso Pesado  ─  1000 solicitudes/día         ║
║  5. Caso Personalizado                           ║
║  6. Salir                                        ║
╚══════════════════════════════════════════════════╝
```

| Opción | Descripción |
|---|---|
| `0` | Activa/desactiva el monitoreo detallado en terminal (logs con TID, estado, válvulas) |
| `1` | Simula 30 días con tráfico ligero (50 solicitudes/día máximo) |
| `2` | Simula 30 días con tráfico moderado (150 solicitudes/día máximo) |
| `3` | **Caso base del enunciado**: 250 solicitudes/día según la normativa |
| `4` | Simula alta contención con 1000 solicitudes/día |
| `5` | Permite ingresar un número personalizado de solicitudes |
| `6` | Sale del programa |

> **Nota**: Se pueden ejecutar múltiples simulaciones consecutivas sin reiniciar el programa.

---

## 📊 Reporte Final (Resultados)

Al finalizar cada simulación de 30 días, el sistema muestra:

- 💧 **Total de metros cúbicos procesados** (litros / 1000)
- 🚨 **Amonestaciones digitales** por cancelaciones inválidas
- ⚠️ **Señales de consumo crítico** (> 500 L) vs. **estándar**
- 🔍 **Total de consultas de presión** (operaciones de solo lectura)
- ⚙️ **Eficiencia del sistema**: nodos ocupados, tiempo de espera promedio, máximo y global
- 💻 **Reporte de recursos de hardware** (CPU y RAM, solo en Linux)

---

## 🔧 Parámetros Configurables

Los parámetros se encuentran en [`include/eco_config.h`](include/eco_config.h):

| Parámetro | Valor por defecto | Descripción |
|---|---|---|
| `NUM_VALVULAS` | 10 | Número de nodos de flujo (válvulas) |
| `DURACION_SIMULACION` | 3 (seg) | Segundos reales que representan 12 horas simuladas (06:00-18:00) |
| `MAX_SOLICITUDES_DIA` | 250 | Solicitudes máximas por día (caso base) |
| `LITROS_CRITICOS` | 500.0 | Umbral para consumo considerado "crítico" |
| `TAM_COLA_AUDITOR` | 50 | Tamaño del buffer circular del Auditor |
| `TASA_REFRESCO_UI` | 500000 (μs) | Intervalo de refresco de la telemetría |

> El **escalado temporal** se calcula automáticamente: al cambiar `DURACION_SIMULACION`, todos los tiempos internos y estadísticas se ajustan mediante `FACTOR_ESCALA`.

---

## 🧵 Patrones de Concurrencia

### 1. Exclusión Mutua (Escritor Estricto)
Cada nodo tiene un semáforo `sem_escritura` que actúa como candado binario. Al reservar, el hilo adquiere el candado y lo retiene durante toda la fase de consumo, liberándolo solo al devolver el nodo.

### 2. Lectores/Escritores con Fairness
Se implementa el patrón clásico Lectores/Escritores con un semáforo adicional (`sem_turno`) que actúa como torniquete. Esto evita que un flujo constante de lectores impida a los escritores acceder (inanición).

### 3. Productor-Consumidor (Auditor)
Los consumos críticos (> 500 L) se encolan en un buffer circular con 3 semáforos:
- `sem_huecos`: espacios disponibles (inicializado a `TAM_COLA_AUDITOR`)
- `sem_elementos`: elementos por procesar (inicializado a 0)
- `sem_mutex`: exclusión mutua para `head`/`tail`

### 4. Apagado Cooperativo
Se usa `_Atomic bool` en lugar de `pthread_cancel` para garantizar un apagado limpio sin riesgo de deadlocks por semáforos retenidos.

---

## 📝 Licencia

Proyecto académico de Sistemas Operativos — Facultad Experimental de Ciencias y Tecnología, Universidad de Carabobo, 2026.

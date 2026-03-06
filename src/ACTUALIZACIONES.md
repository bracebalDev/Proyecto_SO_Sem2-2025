Estabilización Final (Cumplimiento C11/POSIX, Fugas de Memoria y Cross-Platform)
Descripción General:
Este PR contiene los últimos parches críticos de bajo nivel exigidos para que el motor de simulación pase auditorías estrictas de código (cero advertencias, cero fugas de memoria) y pueda compilarse tanto en Windows como en Linux sin colapsar.

🛡️ Cumplimiento de Estándares (C11 y POSIX)
Corrección de Límite en usleep (POSIX): Se modificó el cálculo de tiempos de llegada en las horas pico. POSIX prohíbe pasar más de 1,000,000 μs a la función usleep (comportamiento indefinido). Ahora el sistema detecta si el tiempo es mayor a 1 segundo y lo divide usando sleep() para los segundos enteros y usleep() para la fracción restante.

Resolución de Data Races (Estándar C11): Se cambió la declaración de la bandera de apagado limpio simulacion_activa de volatile bool a _Atomic bool. Esto evita condiciones de carrera a nivel de memoria cuando el main la apaga y los hilos daemon (auditor/telemetría) la leen simultáneamente.

🐛 Correcciones de Memoria y Warnings
Cierre de Fuga de Recursos (pthreads): Se agregó la llamada a pthread_attr_destroy(&attr) al final del ciclo de creación de hilos diarios. Esto evita que la librería de hilos deje "basura" en la memoria del sistema por cada día simulado.

Trazabilidad de Sanciones (Warning Resuelto): Se solucionó la advertencia del compilador (unused parameter ‘uid’) integrando el ID del usuario directamente en el log_evento cuando se aplica una amonestación digital.

🛠️ Mejoras de Compilación (Soporte Cross-Platform)
Soporte para compilación en Windows: Se protegieron las librerías exclusivas de Linux (<sys/resource.h>) y la función de reporte de hardware usando directivas del preprocesador (#ifdef __linux__). Ahora el código se puede ejecutar y probar nativamente en Windows sin errores, pero mostrará el reporte completo automáticamente cuando se defienda en Linux.
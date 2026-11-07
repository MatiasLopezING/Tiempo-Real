/*
 * traza_switch.h  --  instrumentacion de los cambios de contexto
 *
 * Este archivo se fuerza en TODAS las unidades de compilacion con la bandera
 *
 *     -include "${platformio.include_dir}/traza_switch.h"
 *
 * de platformio.ini, de modo que se procesa ANTES que Arduino_FreeRTOS.h.
 * Alli el gancho esta declarado asi:
 *
 *     #ifndef traceTASK_SWITCHED_IN
 *         #define traceTASK_SWITCHED_IN()
 *     #endif
 *
 * Como la guarda es #ifndef, la definicion de abajo es la que queda y la
 * biblioteca no se modifica: ni un archivo suyo cambia de contenido ni de
 * fecha. Es el mismo mecanismo que ya se usa para SERIAL_TX_BUFFER_SIZE.
 *
 * El nucleo invoca el gancho dentro de vTaskSwitchContext(), justo despues de
 * elegir la proxima tarea y con pxCurrentTCB ya apuntando a ella. Es el unico
 * punto del sistema que conoce el instante exacto en que cambia el dueno del
 * procesador, que es precisamente lo que el analizador logico tiene que
 * mostrar.
 */

#ifndef TRAZA_SWITCH_H
#define TRAZA_SWITCH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Definida en main.cpp. Recibe el TCB de la tarea que acaba de tomar el
   procesador; un TaskHandle_t es exactamente ese puntero. */
void marcar_switch_in(void *tcb);

#ifdef __cplusplus
}
#endif

#define traceTASK_SWITCHED_IN()   marcar_switch_in((void *) pxCurrentTCB)

#endif /* TRAZA_SWITCH_H */

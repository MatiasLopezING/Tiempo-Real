/*
 * STR - Practica 3 - Ejercicios 4 y 5 (Round Robin / FIFO)
 * ATmega328P @ 16 MHz, Arduino_FreeRTOS, Proteus
 *
 * Analizador logico:  PB0 -> A0 (tarea A)   PB1 -> A1 (tarea B)
 *                     PB2 -> A2 (tarea C)   PB3 -> A3 (tarea D)
 *
 * Cada pin esta en alto SOLO mientras su tarea tiene la CPU: cada tarea
 * escribe su propio pin (y apaga los demas) en cada milisegundo de computo.
 * Round Robin : configUSE_TIME_SLICING 1   (FreeRTOSConfig.h de la libreria)
 * FIFO        : configUSE_TIME_SLICING 0   (y recompilar)
 * Escala: 1 unidad = 4 ticks = 64 ms (tick real = 16 ms).
 */
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <util/delay_basic.h>

#define PILA_TAREA         144        /* bytes de pila por tarea (RAM escasa) */
#define PILA_REPORTE       256
#define TICKS_POR_UNIDAD   4
#define PRIO_PERIODICAS    2          /* las 4 con IGUAL prioridad */
#define PRIO_REPORTE       3          /* > periodicas: imprime UNA vez */
#define PERIODO_REPORTE_MS 3072       /* = 2 hiperperiodos */
#define ITER_POR_MS        ((uint16_t)(F_CPU / 1000UL / 4UL))   /* 4000 */

typedef struct {
  const char *nombre;
  uint8_t     pin;                    /* bit de PORTB */
  uint8_t     C_unidades, T_unidades;
  uint32_t    activaciones, perdidos;
  uint16_t    resp_max, jitter_max;   /* en ticks */
} tarea_t;

static tarea_t tareas[] = {
  { "A", PB0, 2, 12, 0, 0, 0, 0 },
  { "B", PB1, 2, 24, 0, 0, 0, 0 },
  { "C", PB2, 2,  6, 0, 0, 0, 0 },
  { "D", PB3, 1,  3, 0, 0, 0, 0 },
};
#define N_TAREAS (sizeof(tareas) / sizeof(tareas[0]))

static void quemar_cpu_ms(uint16_t ms, uint8_t mascara)
{
  while (ms--) {
    PORTB = (uint8_t)((PORTB & 0xF0) | mascara);
    _delay_loop_2(ITER_POR_MS);
  }
}

static void vTareaPeriodica(void *pv)
{
  tarea_t *t = (tarea_t *)pv;
  const TickType_t periodo = (TickType_t)t->T_unidades * TICKS_POR_UNIDAD;
  const uint16_t   computo = (uint16_t)t->C_unidades * TICKS_POR_UNIDAD
                                                     * portTICK_PERIOD_MS;
  const uint8_t    mascara = (uint8_t)(1 << t->pin);
  TickType_t prev     = 0;            /* todas activadas en t = 0 */
  TickType_t esperado = 0;

  for (;;) {
    const uint16_t j = (uint16_t)(xTaskGetTickCount() - esperado);
    if (j > t->jitter_max) t->jitter_max = j;

    quemar_cpu_ms(computo, mascara);
    PORTB &= (uint8_t)~mascara;                       /* fin de la rafaga */

    const uint16_t resp = (uint16_t)(xTaskGetTickCount() - esperado);
    if (resp > t->resp_max) t->resp_max = resp;
    if (resp > periodo)     t->perdidos++;
    t->activaciones++;

    esperado += periodo;
    xTaskDelayUntil(&prev, periodo);
  }
}

/* Prioridad MAYOR que las periodicas, pero imprime una sola vez a los 3072 ms.
 * Mientras imprime las periodicas no corren, asi los contadores son consistentes. */
static void vReporte(void *pv)
{
  (void)pv;
  TickType_t prev = 0;
  xTaskDelayUntil(&prev, pdMS_TO_TICKS(PERIODO_REPORTE_MS));

  Serial.println(F("tarea activ perdidos Rmax(ms) jitter(ms)"));
  for (uint8_t i = 0; i < N_TAREAS; i++) {
    Serial.print(tareas[i].nombre);                       Serial.print(' ');
    Serial.print(tareas[i].activaciones);                 Serial.print(' ');
    Serial.print(tareas[i].perdidos);                     Serial.print(' ');
    Serial.print((uint32_t)tareas[i].resp_max * portTICK_PERIOD_MS);   Serial.print(' ');
    Serial.println((uint32_t)tareas[i].jitter_max * portTICK_PERIOD_MS);
  }
  for (;;) vTaskDelay(portMAX_DELAY);
}

void setup()
{
  Serial.begin(9600);
  DDRB  |= 0x0F;                       /* PB0..PB3 salidas */
  PORTB &= 0xF0;

  Serial.print(F("tick="));      Serial.print((uint32_t)portTICK_PERIOD_MS);
  Serial.print(F("ms  unidad="));
  Serial.print((uint32_t)TICKS_POR_UNIDAD * portTICK_PERIOD_MS);
  Serial.println(F("ms"));

  for (uint8_t i = 0; i < N_TAREAS; i++)
    xTaskCreate(vTareaPeriodica, tareas[i].nombre, PILA_TAREA,
                (void *)&tareas[i], PRIO_PERIODICAS, NULL);
  xTaskCreate(vReporte, "REP", PILA_REPORTE, NULL, PRIO_REPORTE, NULL);
}

void loop() { }
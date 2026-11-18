/* Ejercicio 5 - Planificación FIFO (4 tareas de igual prioridad, sin time slicing)
 * ATmega328P @ 16 MHz, Arduino_FreeRTOS. Tick = 15 ms (WDT).
 * PB0=A, PB1=B, PB2=C, PB3=D. PB4 = marcador de hiperperíodo (disparador del analizador).
 * UART 9600 -> Virtual Terminal. 1 unidad de la Parte I = 4 ticks = 60 ms.
 */
#include <Arduino.h>
#include <Arduino_FreeRTOS.h>

#if configUSE_PREEMPTION != 1
#error "configUSE_PREEMPTION debe ser 1"
#endif
#if configUSE_TIME_SLICING != 0
#error "Ej.5: configUSE_TIME_SLICING debe ser 0 en FreeRTOSConfig.h"
#endif
#if configUSE_TICK_HOOK != 1
#error "configUSE_TICK_HOOK debe ser 1 en FreeRTOSConfig.h"
#endif

#define ESPERA_INICIAL_MS 3000             /* tiempo (simulado) para armar Capture; 0 = sin espera */

#define UNIT          4                    /* ticks por unidad de la Parte I */
#define HP_TICKS      (24 * UNIT)          /* hiperperíodo = 96 ticks = 1,44 s */
#define REPORT_TICKS  (4 * HP_TICKS)
#define N_TAREAS      4
#define PRIO_TAREAS   2
#define PRIO_REPORTE  1
#define STACK_TAREA   100
#define STACK_REPORTE 160
#define SYNC_BIT      _BV(PB4)

typedef struct {
  const char   *name;
  uint8_t       mask;
  uint8_t       C;               /* cómputo [ticks] */
  uint16_t      T;               /* período = plazo [ticks] */
  TaskHandle_t  h;
  volatile uint8_t cpu;          /* ticks de CPU consumidos (tick hook) */
  uint16_t      act, miss;
  uint16_t      rmax, jmax;      /* [ticks] */
} Tarea;

static Tarea tareas[N_TAREAS] = {
  { "A", _BV(PB0),  2 * UNIT, 12 * UNIT },
  { "B", _BV(PB1),  2 * UNIT, 24 * UNIT },
  { "C", _BV(PB2),  2 * UNIT,  6 * UNIT },
  { "D", _BV(PB3),  1 * UNIT,  3 * UNIT },
};

/* Tick hook (ISR, interrupciones deshabilitadas):
 *  - acredita el tick a la tarea que corría;
 *  - genera el marcador PB4: alto durante 1 tick cada HP_TICKS (primer flanco en t = HP). */
#ifdef __cplusplus
extern "C"
#endif
void vApplicationTickHook(void)
{
  static uint8_t n = 0;
  n++;
  if (n == HP_TICKS) { n = 0; PORTB |= SYNC_BIT; }
  else if (n == 1)   { PORTB &= ~SYNC_BIT; }

  TaskHandle_t h = xTaskGetCurrentTaskHandle();
  for (uint8_t i = 0; i < N_TAREAS; i++) {
    if (tareas[i].h == h) { tareas[i].cpu++; return; }
  }
}

static void vTareaPeriodica(void *pv)
{
  Tarea *t = (Tarea *)pv;
  TickType_t release = 0;

  for (;;) {                                           /* se repite indefinidamente */
    TickType_t start = xTaskGetTickCount();
    uint8_t c0 = t->cpu;

    taskENTER_CRITICAL(); PORTB |= t->mask;  taskEXIT_CRITICAL();
    while ((uint8_t)(t->cpu - c0) < t->C) { }          /* espera activa: C ticks de CPU propia */
    taskENTER_CRITICAL(); PORTB &= ~t->mask; taskEXIT_CRITICAL();

    TickType_t end = xTaskGetTickCount();
    uint16_t R = (uint16_t)(end - release);
    uint16_t J = (uint16_t)(start - release);

    taskENTER_CRITICAL();
    t->act++;
    if (R > t->T)    t->miss++;
    if (R > t->rmax) t->rmax = R;
    if (J > t->jmax) t->jmax = J;
    taskEXIT_CRITICAL();

    TickType_t prev = release;
    release += t->T;
    TickType_t d = (TickType_t)(release - xTaskGetTickCount());
    if (d != 0 && d <= (TickType_t)(((TickType_t)-1) / 2)) {
      vTaskDelayUntil(&prev, t->T);
    } else {
      taskYIELD();                                     /* trabajo atrasado: al final de la cola */
    }
  }
}

static void vReporte(void *pv)
{
  (void)pv;
  for (;;) {
    vTaskDelay(REPORT_TICKS);
    Serial.print(F("--- t=")); Serial.print(xTaskGetTickCount()); Serial.println(F(" ticks"));
    for (uint8_t i = 0; i < N_TAREAS; i++) {
      uint16_t a, m, r, j;
      taskENTER_CRITICAL();
      a = tareas[i].act; m = tareas[i].miss; r = tareas[i].rmax; j = tareas[i].jmax;
      taskEXIT_CRITICAL();
      Serial.print(tareas[i].name);
      Serial.print(F(": act="));  Serial.print(a);
      Serial.print(F(" miss="));  Serial.print(m);
      Serial.print(F(" Rmax="));  Serial.print(r);
      Serial.print(F(" Jmax="));  Serial.println(j);
    }
  }
}

void setup(void)
{
  DDRB  |= 0x1F;                 /* PB0..PB4 como salida */
  PORTB &= ~0x1F;
  Serial.begin(9600);

  for (uint8_t i = 0; i < N_TAREAS; i++) {
    if (xTaskCreate(vTareaPeriodica, tareas[i].name, STACK_TAREA,
                    &tareas[i], PRIO_TAREAS, &tareas[i].h) != pdPASS) {
      Serial.println(F("ERR: sin heap")); for (;;) { }
    }
  }
  if (xTaskCreate(vReporte, "REP", STACK_REPORTE, NULL, PRIO_REPORTE, NULL) != pdPASS) {
    Serial.println(F("ERR: sin heap")); for (;;) { }
  }

  Serial.println(F("Listo. Armar Capture ahora..."));
  Serial.flush();
  delay(ESPERA_INICIAL_MS);      /* Timer0 (millis); el scheduler todavía no arrancó */

  PORTB |= SYNC_BIT;             /* marcador en t = 0; el hook lo baja en el tick 1 */

  #define BURN_MS 2000UL

  /* Quema CPU 2 s con PC0 en alto (pin 23). El flanco de bajada marca el t = 0 del scheduler. */
  Serial.println(F("Quemando CPU 2 s. Armar Capture ahora..."));
  Serial.flush();
  DDRC  |= _BV(PC0);
  PORTC |= _BV(PC0);
  uint32_t t0 = millis();                  /* Timer0; el scheduler todavía no arrancó */
  while ((millis() - t0) < BURN_MS) { }    /* espera activa, sin delay() */
  PORTC &= ~_BV(PC0);

  vTaskStartScheduler();
}

void loop(void) { }
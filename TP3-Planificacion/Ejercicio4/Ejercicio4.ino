/*
 * Sistemas de Tiempo Real - Practica 3 - Ejercicio 4 (Round Robin)
 *
 * Cuatro tareas periodicas del Ejercicio 1 con IGUAL PRIORIDAD y
 * configUSE_TIME_SLICING habilitado (Round Robin entre tareas de
 * igual prioridad, quantum = 1 tick = 16 ms).
 *
 * Tarea  C (unid.)  T (unid.)   Pin
 *   A       2          12       PB0
 *   B       2          24       PB1
 *   C       2           6       PB2
 *   D       1           3       PB3
 *
 * Target  : ATmega328P @ 16 MHz (CKDIV8 sin programar), Proteus 8
 * Pines   : PB0=A, PB1=B, PB2=C, PB3=D  -> canales 0..3 del Logic Analyser
 * Serie   : USART0 -> PD1 (TXD), 9600 8N1 -> Virtual Terminal
 * Libreria: Arduino_FreeRTOS (feilipu)
 *
 * ESCALA DE TIEMPOS
 *   1 unidad del enunciado = 4 ticks.
 *   portTICK_PERIOD_MS = 16 ms (Timer1 CTC exactamente a 16,00 ms).
 *   => 1 unidad = 4 * 16 = 64 ms reales.
 *   Hiperperiodo H = mcm(12, 24, 6, 3) = 24 unidades = 96 ticks = 1536 ms.
 *
 * CONFIGURACION REQUERIDA EN FreeRTOSConfig.h:
 *   #define configUSE_PREEMPTION    1
 *   #define configUSE_TIME_SLICING  1
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <util/delay_basic.h>
#include <util/delay.h>

#define TICKS_POR_UNIDAD   4

/* ---------------- calibracion del banco de medicion --------------------- */
#define TICK_NUM  1600          /* 16,00 ms */
#define TICK_DEN   100
#define TICKS_A_MS(n)  ((uint16_t)(((uint32_t)(n) * TICK_NUM) / TICK_DEN))

#define PRIO_PERIODICAS       2 /* TODAS con la MISMA prioridad (Round Robin) */
#define PRIO_REPORTE          1
#define PERIODO_REPORTE_TICKS (24U * TICKS_POR_UNIDAD) /* 96 ticks = 1536 ms */

/* Tamano de pila en bytes (ATmega328P tiene solo 2048 B de SRAM).
 * Con 4 tareas periodicas + REP + IDLE + Tmr Svc, usar 192 y 400 bytes
 * agotaria el heap (vApplicationMallocFailedHook). Con 110 y 160 bytes
 * las 5 tareas caben comodamente dejando ~260 bytes libres de heap.        */
#define PILA_TAREA   110
#define PILA_REPORTE 160

/* ---------------- consumo de CPU por espera activa ----------------------
 * _delay_loop_2 consume exactamente 4 ciclos por iteracion. El lazo cuenta
 * TRABAJO EJECUTADO, no tiempo de reloj: si la tarea es expropiada en el
 * medio de un tick, el contador no avanza mientras no corre y la rafaga se
 * reanuda donde habia quedado.
 * En cada subpaso (~0,1 ms) se actualiza PORTB para mantener en alto
 * UNICAMENTE el pin de la tarea que posee la CPU en ese instante; asi el
 * Logic Analyser muestra cada quantum individual de 1 tick (16 ms).       */
#define MASK_TAREAS     ((uint8_t)(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3)))
#define SUBPASOS_POR_MS 10
#define ITER_SUBPASO    396U

static void quemar_cpu_ms(uint16_t ms, uint8_t pin)
{
  const uint8_t bit = (uint8_t)(1U << pin);
  uint16_t pasos = ms * SUBPASOS_POR_MS;
  while (pasos--) {
    PORTB = (uint8_t)((PORTB & ~MASK_TAREAS) | bit);
    _delay_loop_2(ITER_SUBPASO);
  }
  PORTB &= (uint8_t)~bit;
}

/* ---------------- RAM libre (diagnostico) ------------------------------- */
static int ram_libre(void)
{
  extern int __heap_start;
  extern int *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

/* ---------------- descripcion y estadisticas de una tarea --------------- */
typedef struct {
  const char *nombre;
  uint8_t     pin;            /* bit de PORTB */
  uint8_t     C_unidades;
  uint8_t     T_unidades;
  uint32_t    activaciones;
  uint32_t    perdidos;       /* plazos perdidos (D = T) */
  uint16_t    resp_max;       /* tiempo de respuesta maximo, en ticks */
  uint16_t    jitter_max;     /* jitter de activacion maximo, en ticks */
  uint16_t    per_ticks;      /* ticks entre dos activaciones consecutivas */
  uint16_t    dt_rafaga;      /* ticks transcurridos durante la rafaga     */
} tarea_t;

/* Ejercicio 4: las cuatro tareas del Ejercicio 1, TODAS con igual prioridad */
static tarea_t tareas[] = {
  { "A", PB0, 2, 12, 0, 0, 0, 0, 0, 0 },
  { "B", PB1, 2, 24, 0, 0, 0, 0, 0, 0 },
  { "C", PB2, 2,  6, 0, 0, 0, 0, 0, 0 },
  { "D", PB3, 1,  3, 0, 0, 0, 0, 0, 0 },
};
#define N_TAREAS (sizeof(tareas) / sizeof(tareas[0]))

/* ---------------- cuerpo comun de las tareas periodicas ----------------- */
static void vTareaPeriodica(void *pv)
{
  tarea_t *t = (tarea_t *)pv;

  const TickType_t periodo = (TickType_t)t->T_unidades * TICKS_POR_UNIDAD;
  const uint16_t   computo = TICKS_A_MS((uint16_t)t->C_unidades
                                        * TICKS_POR_UNIDAD);

  TickType_t prev        = xTaskGetTickCount();
  TickType_t esperado    = prev;              /* instante teorico de activacion */
  TickType_t inicio_prev = 0;
  uint8_t    primera     = 1;

  for (;;) {
    const TickType_t inicio = xTaskGetTickCount();

    if (!primera) t->per_ticks = (uint16_t)(inicio - inicio_prev);
    primera     = 0;
    inicio_prev = inicio;

    /* jitter: desvio entre el inicio de ejecucion real y la activacion teorica */
    const uint16_t j = (uint16_t)(inicio - esperado);
    if (j > t->jitter_max) t->jitter_max = j;

    quemar_cpu_ms(computo, t->pin);           /* ejecuta rafaga marcando PBx */

    /* Se suma 1 tick porque la rafaga concluye justo al final del tick en curso */
    const TickType_t fin = xTaskGetTickCount() + 1U;
    t->dt_rafaga = (uint16_t)(fin - inicio);

    /* tiempo de respuesta: desde la activacion teorica hasta el fin */
    const uint16_t resp = (uint16_t)(fin - esperado);
    if (resp > t->resp_max) t->resp_max = resp;
    if (resp > periodo)     t->perdidos++;    /* plazo D = T = periodo */
    t->activaciones++;

    /* Con el fix de port.c (reti directo en ISR), el tick avanza normalmente
     * durante quemar_cpu_ms (dtT > 0) y expropia cada 1 tick en Round Robin.
     * Por eso xTaskDelayUntil recibe el periodo completo T (medido desde prev). */
    esperado += periodo;
    xTaskDelayUntil(&prev, periodo);
  }
}

/* ---------------- tarea de reporte (menor prioridad) -------------------- */
static void vReporte(void *pv)
{
  (void)pv;
  TickType_t prev = xTaskGetTickCount();

  Serial.println(F("T act perd Rmax Jmax perT dtT"));

  for (;;) {
    /* Reporta al final de cada hiperperiodo (96 ticks), aprovechando los
     * 8 ticks libres [88..96) en que A, B, C y D terminaron todas sus rafagas. */
    xTaskDelayUntil(&prev, (TickType_t)PERIODO_REPORTE_TICKS);

    Serial.println(F("-- Hiperperiodo --"));
    for (uint8_t i = 0; i < N_TAREAS; i++) {
      Serial.print(tareas[i].nombre);                 Serial.print(' ');
      Serial.print((uint16_t)tareas[i].activaciones); Serial.print(' ');
      Serial.print((uint16_t)tareas[i].perdidos);     Serial.print(' ');
      Serial.print(TICKS_A_MS(tareas[i].resp_max));   Serial.print(' ');
      Serial.print(TICKS_A_MS(tareas[i].jitter_max)); Serial.print(' ');
      Serial.print(tareas[i].per_ticks);              Serial.print(' ');
      Serial.println(tareas[i].dt_rafaga);
    }
  }
}

/* ---------------------------- arranque ---------------------------------- */
void setup()
{
  /* Deshabilita Timer0 de Arduino (millis) para no robar ciclos al lazo */
  TIMSK0 = 0;

  Serial.begin(9600);

  DDRB  |=  (uint8_t)(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));
  PORTB &= (uint8_t)~(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));

  Serial.print(F("tick="));      Serial.print(TICKS_A_MS(1));
  Serial.print(F("ms  unidad="));
  Serial.print(TICKS_A_MS(TICKS_POR_UNIDAD));
  Serial.println(F("ms"));

  for (uint8_t i = 0; i < N_TAREAS; i++) {
    xTaskCreate(vTareaPeriodica, tareas[i].nombre, PILA_TAREA,
                (void *)&tareas[i], PRIO_PERIODICAS, NULL);
  }

  xTaskCreate(vReporte, "REP", PILA_REPORTE, NULL, PRIO_REPORTE, NULL);

  Serial.print(F("RAM libre=")); Serial.println(ram_libre());
}

void loop() { }

/* ---------------- hooks de error de FreeRTOS --------------------------- */
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask,
                                              char *pcTaskName)
{
  (void)xTask;
  for (;;) {
    Serial.print(F("*** PILA: "));
    Serial.println(pcTaskName ? pcTaskName : "?");
    Serial.flush();
    _delay_ms(2000);
  }
}

extern "C" void vApplicationMallocFailedHook(void)
{
  for (;;) {
    Serial.println(F("*** HEAP AGOTADO"));
    Serial.flush();
    _delay_ms(2000);
  }
}

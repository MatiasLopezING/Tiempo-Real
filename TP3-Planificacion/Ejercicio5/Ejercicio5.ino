/*
 * Sistemas de Tiempo Real - Practica 3 - Ejercicio 3 (banco de medicion)
 *
 * Una unica tarea periodica: la tarea D del Ejercicio 1 (C = 1, T = 3 unidades).
 * Marca PB3 en alto mientras consume CPU y en bajo al terminar la rafaga.
 * Una tarea de menor prioridad reporta por la terminal.
 *
 * Target  : ATmega328P @ 16 MHz (CKDIV8 sin programar), Proteus 8
 * Pines   : PB0=A, PB1=B, PB2=C, PB3=D  -> canales 0..3 del Logic Analyser
 * Serie   : USART0 -> PD1 (TXD), 9600 8N1 -> Virtual Terminal
 * Libreria: Arduino_FreeRTOS (feilipu)
 *
 * ESCALA DE TIEMPOS
 *   1 unidad del enunciado = 4 ticks.
 *   El tick real de esta libreria es portTICK_PERIOD_MS = 16 ms (watchdog),
 *   no los 15 ms nominales del enunciado. Por eso todo se expresa en TICKS:
 *   asi los periodos y los computos quedan como multiplos exactos del tick,
 *   que es lo que pide el enunciado.
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <util/delay_basic.h>

/* MODO_CAPTURA = 0 -> PB0..PB3 muestran QUIEN TIENE LA CPU (via traceTASK_SWITCHED_IN)
 * MODO_CAPTURA = 1 -> PB0..PB3 conmutan (toggle) al TERMINAR cada rafaga       */
#ifndef MODO_CAPTURA
#define MODO_CAPTURA       0
#endif

#define TICKS_POR_UNIDAD   4
#define PRIO_PERIODICAS    2
#define PRIO_REPORTE       1
#define PERIODO_REPORTE_MS 3072   /* = 2 hiperperiodos = 192 ticks */

/* ---------------- consumo de CPU por espera activa ----------------------
 * _delay_loop_2 consume exactamente 4 ciclos por iteracion. El lazo cuenta
 * TRABAJO EJECUTADO, no tiempo de reloj: si la tarea es expropiada en el
 * medio, el contador no avanza mientras no corre y la rafaga se reanuda
 * donde habia quedado.                                                   */
#define ITER_POR_MS   ((uint16_t)(F_CPU / 1000UL / 4UL))   /* 4000 @16MHz */

static void quemar_cpu_ms(uint16_t ms)
{
  while (ms--) _delay_loop_2(ITER_POR_MS);
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
} tarea_t;

/* Ejercicio 3: una sola tarea activa. Para el Ejercicio 4 se agregan
   A, B y C a esta misma tabla.                                           */
static tarea_t tareas[] = {
  { "A", PB0, 2, 12, 0, 0, 0, 0 },
  { "B", PB1, 2, 24, 0, 0, 0, 0 },
  { "C", PB2, 2,  6, 0, 0, 0, 0 },
  { "D", PB3, 1,  3, 0, 0, 0, 0 },
};

/* Llamada desde traceTASK_SWITCHED_IN() (FreeRTOSConfig.h). Corre dentro del
 * cambio de contexto, con interrupciones deshabilitadas: mantenerla minima.  */
extern "C" void trazaCambio(char c)
{
#if MODO_CAPTURA == 0
  uint8_t m = (c >= 'A' && c <= 'D') ? (uint8_t)(1 << (c - 'A')) : 0;
  PORTB = (uint8_t)((PORTB & ~0x0F) | m);
#else
  (void)c;
#endif
}
#define N_TAREAS (sizeof(tareas) / sizeof(tareas[0]))

/* ---------------- cuerpo comun de las tareas periodicas ----------------- */
static void vTareaPeriodica(void *pv)
{
  tarea_t *t = (tarea_t *)pv;

  const TickType_t periodo  = (TickType_t)t->T_unidades * TICKS_POR_UNIDAD;
  const uint16_t   computo  = (uint16_t)t->C_unidades * TICKS_POR_UNIDAD
                                                      * portTICK_PERIOD_MS;
  TickType_t prev     = 0;     /* activacion simultanea en t=0 para TODAS */
  TickType_t esperado = prev;                 /* instante teorico de activacion */

  for (;;) {
    const TickType_t inicio = xTaskGetTickCount();

    /* jitter: desvio entre la activacion real y la teorica */
    const uint16_t j = (uint16_t)(inicio - esperado);
    if (j > t->jitter_max) t->jitter_max = j;

    quemar_cpu_ms(computo);                   /* el pin de ocupacion lo maneja el kernel */
#if MODO_CAPTURA == 1
    PINB = (uint8_t)(1 << t->pin);            /* toggle: marca de fin de rafaga */
#endif

    /* tiempo de respuesta: desde la activacion teorica hasta el fin */
    const uint16_t resp = (uint16_t)(xTaskGetTickCount() - esperado);
    if (resp > t->resp_max) t->resp_max = resp;
    if (resp > periodo)     t->perdidos++;    /* plazo = periodo */
    t->activaciones++;

    esperado += periodo;
    xTaskDelayUntil(&prev, periodo);          /* absoluto: no acumula deriva */
  }
}

/* ---------------- tarea de reporte (menor prioridad) -------------------- */
static void vReporte(void *pv)
{
  (void)pv;
  TickType_t prev = xTaskGetTickCount();

  for (;;) {
    xTaskDelayUntil(&prev, pdMS_TO_TICKS(PERIODO_REPORTE_MS));

    Serial.println(F("--- tarea  activ  perdidos  Rmax(ms)  jitter(ms) ---"));
    for (uint8_t i = 0; i < N_TAREAS; i++) {
      Serial.print(F("    "));   Serial.print(tareas[i].nombre);
      Serial.print(F("      ")); Serial.print(tareas[i].activaciones);
      Serial.print(F("        ")); Serial.print(tareas[i].perdidos);
      Serial.print(F("       "));
      Serial.print((uint32_t)tareas[i].resp_max * portTICK_PERIOD_MS);
      Serial.print(F("        "));
      Serial.println((uint32_t)tareas[i].jitter_max * portTICK_PERIOD_MS);
    }
  }
}

/* ---------------------------- arranque ---------------------------------- */
void setup()
{
  Serial.begin(9600);

  DDRB  |=  (uint8_t)(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));
  PORTB &= (uint8_t)~(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));

  Serial.print(F("tick="));      Serial.print((uint32_t)portTICK_PERIOD_MS);
  Serial.print(F("ms  unidad=")); 
  Serial.print((uint32_t)TICKS_POR_UNIDAD * portTICK_PERIOD_MS);
  Serial.println(F("ms"));

  for (uint8_t i = 0; i < N_TAREAS; i++)
    xTaskCreate(vTareaPeriodica, tareas[i].nombre, 192,
                (void *)&tareas[i], PRIO_PERIODICAS, NULL);

  xTaskCreate(vReporte, "REP", 256, NULL, PRIO_REPORTE, NULL);
}

void loop() { }

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
 *   El tick lo genera el Watchdog y vale portTICK_PERIOD_MS = 16 ms, no los
 *   15 ms nominales del enunciado; se verifico con el analizador logico.
 *   Por eso todo se expresa en TICKS: asi los periodos y los computos
 *   quedan como multiplos exactos del tick, que es lo que pide el
 *   enunciado. Para la tarea D resulta C = 64 ms y T = 192 ms.
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <util/delay_basic.h>
#include <util/delay.h>

#define TICKS_POR_UNIDAD   4

/* ---------------- calibracion del banco de medicion ---------------------
 * Duracion del tick verificada con el analizador logico: 16,0 ms, que
 * coincide con portTICK_PERIOD_MS. Se deja como fraccion para poder
 * recalibrar sin tocar el resto del codigo.                              */
#define TICK_NUM  1600          /* 16,00 ms */
#define TICK_DEN   100

/* milisegundos reales que corresponden a n ticks */
#define TICKS_A_MS(n)  ((uint16_t)(((uint32_t)(n) * TICK_NUM) / TICK_DEN))
#define PRIO_PERIODICAS    2
#define PRIO_REPORTE       1

/* Periodo del reporte expresado directamente en TICKS.
 * TickType_t es de 16 bits en esta libreria, por eso no se usa
 * pdMS_TO_TICKS() con valores grandes.
 * La tarea D espera 8 ticks por periodo, de modo que el contador de
 * activaciones avanza de a 192 / 8 = 24 por reporte.                   */
#define PERIODO_REPORTE_TICKS 192

/* Tamano de pila de cada tarea, en bytes. El uso medido es ~115 bytes;
 * 192 es el configMINIMAL_STACK_SIZE de la libreria. */
#define PILA_TAREA 192

/* La tarea de reporte lleva mas pila que las periodicas, por margen: cada
 * Serial.print usa un buffer de conversion y la interrupcion del tick se
 * apila sobre la pila de la tarea que esta corriendo (~33 bytes de contexto
 * en AVR). Los hooks del final avisan si aun asi llegara a desbordarse. */
#define PILA_REPORTE 400

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

/* ---------------- RAM libre (diagnostico) -------------------------------
 * Distancia entre el tope del heap y el puntero de pila actual.          */
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

/* Ejercicio 3: una sola tarea activa. Para el Ejercicio 4 se agregan
   A, B y C a esta misma tabla.                                           */
static tarea_t tareas[] = {
  { "D", PB3, 1, 3, 0, 0, 0, 0, 0, 0 },
};
#define N_TAREAS (sizeof(tareas) / sizeof(tareas[0]))

/* ---------------- cuerpo comun de las tareas periodicas ----------------- */
static void vTareaPeriodica(void *pv)
{
  tarea_t *t = (tarea_t *)pv;

  const TickType_t periodo  = (TickType_t)t->T_unidades * TICKS_POR_UNIDAD;
  /* La rafaga debe durar exactamente C unidades = C*4 ticks de tiempo real. */
  const uint16_t   computo  = TICKS_A_MS((uint16_t)t->C_unidades
                                         * TICKS_POR_UNIDAD);
  /* La espera activa NO incrementa el contador de ticks del sistema
     operativo: medido sobre la terminal, dtT = 0 ticks por rafaga. Si se
     pidiera una espera del periodo completo, xTaskDelayUntil la contaria
     entera DESPUES de la rafaga y el periodo real quedaria en C + T. Por eso
     se descuenta la duracion de la rafaga, expresada en ticks.           */
  const TickType_t rafaga = (TickType_t)t->C_unidades * TICKS_POR_UNIDAD;
  const TickType_t espera = (TickType_t)(periodo - rafaga);

  TickType_t prev     = xTaskGetTickCount();
  TickType_t esperado = prev;                 /* instante teorico de activacion */

  TickType_t inicio_prev = 0;

  for (;;) {
    const TickType_t inicio = xTaskGetTickCount();

    /* Periodo REAL entre activaciones, contado en ticks por el programa. */
    if (inicio_prev) t->per_ticks = (uint16_t)(inicio - inicio_prev);
    inicio_prev = inicio;

    /* jitter: desvio entre la activacion real y la teorica */
    const uint16_t j = (uint16_t)(inicio - esperado);
    if (j > t->jitter_max) t->jitter_max = j;

    PORTB |=  (uint8_t)(1 << t->pin);         /* arranca la rafaga */
    quemar_cpu_ms(computo);
    PORTB &= (uint8_t)~(1 << t->pin);         /* termina la rafaga */

    /* Ticks que el contador avanzo mientras duro la espera activa. */
    t->dt_rafaga = (uint16_t)(xTaskGetTickCount() - inicio);

    /* tiempo de respuesta: desde la activacion teorica hasta el fin */
    const uint16_t resp = (uint16_t)(xTaskGetTickCount() - esperado);
    if (resp > t->resp_max) t->resp_max = resp;
    if (resp > periodo)     t->perdidos++;    /* plazo D = T = periodo */
    t->activaciones++;

    esperado += espera;
    xTaskDelayUntil(&prev, espera);           /* absoluto: no acumula deriva */
  }
}

/* ---------------- tarea de reporte (menor prioridad) -------------------- */
static void vReporte(void *pv)
{
  (void)pv;
  TickType_t prev = xTaskGetTickCount();

  Serial.println(F("REP en marcha"));
  Serial.println(F("tarea activ perdidos Rmax jitter perT dtT"));

  for (;;) {
    xTaskDelayUntil(&prev, (TickType_t)PERIODO_REPORTE_TICKS);

    for (uint8_t i = 0; i < N_TAREAS; i++) {
      /* Una linea corta por tarea. Cada Serial.print se despacha sin llenar
         el buffer de transmision del USART (64 bytes), de modo que la tarea
         nunca queda esperando dentro de la escritura.                      */
      Serial.print(tareas[i].nombre);              Serial.print(' ');
      Serial.print((uint16_t)tareas[i].activaciones); Serial.print(' ');
      Serial.print((uint16_t)tareas[i].perdidos);  Serial.print(' ');
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
  Serial.begin(9600);

  DDRB  |=  (uint8_t)(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));
  PORTB &= (uint8_t)~(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));

  Serial.print(F("tick="));      Serial.print(TICKS_A_MS(1));
  Serial.print(F("ms  unidad="));
  Serial.print(TICKS_A_MS(TICKS_POR_UNIDAD));
  Serial.println(F("ms"));

  Serial.print(F("TickType_t=")); Serial.print((uint16_t)sizeof(TickType_t));
  Serial.print(F("B  RAM libre inicio=")); Serial.println(ram_libre());

  for (uint8_t i = 0; i < N_TAREAS; i++) {
    BaseType_t ok = xTaskCreate(vTareaPeriodica, tareas[i].nombre, PILA_TAREA,
                                (void *)&tareas[i], PRIO_PERIODICAS, NULL);
    Serial.print(F("crear ")); Serial.print(tareas[i].nombre);
    Serial.print(F(" -> ")); Serial.println((int)ok);
  }

  BaseType_t okr = xTaskCreate(vReporte, "REP", PILA_REPORTE, NULL,
                               PRIO_REPORTE, NULL);
  Serial.print(F("crear REP -> ")); Serial.println((int)okr);

  Serial.print(F("RAM libre fin=")); Serial.println(ram_libre());
  Serial.println(F("arranca el planificador"));
}

void loop() {
}

/* ---------------- hooks de error de FreeRTOS ---------------------------
 * Los de la libreria son `weak` y solo parpadean un LED en un lazo infinito,
 * asi que un desborde de pila se ve como un cuelgue mudo. Estos avisan.   */

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

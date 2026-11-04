/*
 * Sistemas de Tiempo Real - Practica 3 - Ejercicios 4 y 5
 * Planificacion Round Robin y FIFO sobre el conjunto de tareas del Ejercicio 1.
 *
 * Target  : ATmega328P @ 16 MHz (CKDIV8 sin programar), Proteus 8
 * Pines   : PB0=A, PB1=B, PB2=C, PB3=D  -> canales 0..3 del Logic Analyser
 * Serie   : USART0 -> PD1 (TXD), 9600 8N1 -> Virtual Terminal
 * Libreria: Arduino_FreeRTOS (feilipu), SIN modificar
 *
 * POR QUE ESTE DISENO NO USA EL TICK DEL SISTEMA OPERATIVO
 *   Medido en el Ejercicio 3: en Proteus el contador de ticks NO avanza
 *   mientras una tarea consume CPU (columna dtT = 0). El tick mide tiempo
 *   ocioso, no tiempo real. Con el conjunto del Ejercicio 1 la utilizacion es
 *   U = 2/12 + 2/24 + 2/6 + 1/3 = 0,9167, de modo que solo 2 de cada 24
 *   unidades son ociosas: el reloj del sistema operativo correria doce veces
 *   mas lento que el tiempo real y no habria ni cuanto ni activaciones
 *   periodicas. Por eso:
 *
 *     - El reloj es `tiempo_ms`, que avanza 1 por cada milisegundo de CPU
 *       realmente consumido. La tarea de sistema consume el tiempo ocioso de
 *       forma calibrada, asi que la suma mide tiempo real.
 *     - El cuanto de Round Robin lo produce la propia tarea con taskYIELD(),
 *       que termina en vTaskSwitchContext() y rota la lista de listos igual
 *       que lo haria el tick.
 *     - Las activaciones periodicas las libera un despachador con
 *       notificaciones de tarea.
 *     - El trabajo de fondo vive en loop(), que la biblioteca ejecuta desde
 *       vApplicationIdleHook(), o sea desde la tarea Idle: es la de menor
 *       prioridad y no gasta una pila extra.
 *
 *   Nada de esto toca la biblioteca.
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <task.h>
#include <util/delay_basic.h>
#include <util/delay.h>

/* ---------------- parametros del experimento --------------------------- */

/* 1 = Ejercicio 4 (Round Robin)   0 = Ejercicio 5 (FIFO, rafagas atomicas) */
#define USAR_ROUND_ROBIN   1

#define MS_POR_UNIDAD      64     /* 1 unidad del enunciado = 4 ticks = 64 ms */
#define QUANTUM_MS         16     /* cuanto = 1 periodo de tick nominal       */
#define MS_REPORTE       3072     /* dos hiperperiodos (hiperperiodo = 1536)  */

#define PRIO_PERIODICAS     2
#define PILA_PERIODICA    160

/* ---------------- reloj de tiempo real ---------------------------------
 * _delay_loop_2 consume exactamente 4 ciclos por iteracion, de modo que cada
 * vuelta del lazo es 1 ms de CPU. El contador avanza solo cuando hay trabajo
 * ejecutado: una tarea expropiada no le suma tiempo.                      */
#define ITER_POR_MS   ((uint16_t)(F_CPU / 1000UL / 4UL))   /* 4000 @16MHz */

static volatile uint32_t tiempo_ms = 0;

static void quemar_cpu_ms(uint16_t ms)
{
  while (ms--) {
    _delay_loop_2(ITER_POR_MS);
    taskENTER_CRITICAL();
    tiempo_ms++;
    taskEXIT_CRITICAL();
  }
}

static uint32_t ahora(void)
{
  uint32_t v;
  taskENTER_CRITICAL();
  v = tiempo_ms;
  taskEXIT_CRITICAL();
  return v;
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
  const char  *nombre;
  uint8_t      pin;          /* bit de PORTB */
  uint16_t     C_ms;         /* tiempo de computo                         */
  uint16_t     T_ms;         /* periodo = plazo                           */
  TaskHandle_t h;
  uint32_t     prox_ms;      /* instante teorico de la proxima activacion */
  uint32_t     act_ms;       /* instante teorico de la activacion en curso*/
  uint8_t      pendiente;    /* 1 = tiene una activacion sin terminar     */
  uint16_t     activaciones;
  uint16_t     perdidos;     /* plazos perdidos (D = T)                   */
  uint16_t     resp_max;     /* tiempo de respuesta maximo, en ms         */
  uint16_t     jitter_max;   /* desvio de activacion maximo, en ms        */
  uint16_t     fin1_ms;      /* instante en que termino la PRIMERA rafaga */
} tarea_t;

/* Conjunto del Ejercicio 1: A(2,12) B(2,24) C(2,6) D(1,3), en unidades. */
static tarea_t tareas[] = {
  { "A", PB0,  2 * MS_POR_UNIDAD, 12 * MS_POR_UNIDAD, NULL, 0, 0, 0, 0, 0, 0, 0, 0 },
  { "B", PB1,  2 * MS_POR_UNIDAD, 24 * MS_POR_UNIDAD, NULL, 0, 0, 0, 0, 0, 0, 0, 0 },
  { "C", PB2,  2 * MS_POR_UNIDAD,  6 * MS_POR_UNIDAD, NULL, 0, 0, 0, 0, 0, 0, 0, 0 },
  { "D", PB3,  1 * MS_POR_UNIDAD,  3 * MS_POR_UNIDAD, NULL, 0, 0, 0, 0, 0, 0, 0, 0 },
};
#define N_TAREAS (sizeof(tareas) / sizeof(tareas[0]))

/* ---------------- despachador de activaciones --------------------------
 * Libera las tareas cuyo instante de activacion ya vencio. Lo llaman tanto la
 * tarea de sistema como las periodicas al final de cada cuanto, de modo que
 * una activacion nunca se atrasa mas de un cuanto, igual que ocurriria con un
 * tick de 16 ms.
 *
 * El planificador se suspende durante el reparto para que las cuatro tareas
 * queden liberadas en el MISMO instante (lo que pide el inciso 4b) y no una
 * por vuelta.                                                             */
static void despachar(void)
{
  const uint32_t t = ahora();

  vTaskSuspendAll();
  for (uint8_t i = 0; i < N_TAREAS; i++) {
    tarea_t *p = &tareas[i];
    if (t >= p->prox_ms) {
      /* Si la activacion anterior sigue pendiente, no termino dentro de su
         plazo: D = T. En ese caso NO se pisa act_ms, para que el tiempo de
         respuesta se siga midiendo desde la activacion que la tarea esta
         ejecutando realmente y no desde la nueva.                         */
      if (p->pendiente) p->perdidos++;
      else              p->act_ms = p->prox_ms;

      const uint16_t j = (uint16_t)(t - p->prox_ms);
      if (j > p->jitter_max) p->jitter_max = j;

      p->prox_ms += p->T_ms;
      p->activaciones++;
      p->pendiente = 1;
      xTaskNotifyGive(p->h);
    }
  }
  xTaskResumeAll();
}

/* ---------------- cuerpo comun de las tareas periodicas ----------------- */
static void vPeriodica(void *pv)
{
  tarea_t *t = (tarea_t *)pv;
  const uint8_t bit = (uint8_t)(1 << t->pin);

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   /* espera su activacion */

    uint16_t restante = t->C_ms;

    PORTB |= bit;                              /* alto mientras usa la CPU */
    while (restante) {
      const uint16_t trozo = (restante > QUANTUM_MS) ? QUANTUM_MS : restante;
      quemar_cpu_ms(trozo);
      restante -= trozo;

      despachar();                             /* libera lo que haya vencido */

#if USAR_ROUND_ROBIN
      if (restante) {
        PORTB &= (uint8_t)~bit;                /* deja la CPU */
        taskYIELD();                           /* fin de cuanto: rota */
        PORTB |= bit;                          /* la recupera */
      }
#endif
    }
    PORTB &= (uint8_t)~bit;

    const uint32_t fin = ahora();

    /* Inciso 4b: instante en que termina la PRIMERA rafaga, con las cuatro
       tareas activadas simultaneamente en t = 0.                          */
    if (t->fin1_ms == 0) t->fin1_ms = (uint16_t)fin;

    const uint16_t resp = (uint16_t)(fin - t->act_ms);
    if (resp > t->resp_max) t->resp_max = resp;
    t->pendiente = 0;
  }
}

/* ---------------- trabajo de fondo: la tarea Idle -----------------------
 * loop() lo ejecuta vApplicationIdleHook(), o sea la tarea Idle, que es la de
 * menor prioridad del sistema y solo corre cuando ninguna periodica esta
 * lista. Se usa en lugar de crear una tarea propia porque en el ATmega328P no
 * sobra RAM para una sexta pila.
 *
 * Hace tres cosas: consume el tiempo ocioso de forma calibrada para que el
 * reloj siga avanzando, despacha activaciones, e imprime el reporte. Imprime
 * una linea por vuelta, de modo que la transmision serie no retrase el
 * despacho mas que eso.                                                   */
void loop()
{
  static uint32_t prox_rep = MS_REPORTE;
  static uint8_t  linea    = N_TAREAS;   /* N_TAREAS = no hay reporte en curso */

  quemar_cpu_ms(1);
  despachar();

  if (linea < N_TAREAS) {
    tarea_t *t = &tareas[linea];
    Serial.print(t->nombre);       Serial.print(' ');
    Serial.print(t->activaciones); Serial.print(' ');
    Serial.print(t->perdidos);     Serial.print(' ');
    Serial.print(t->resp_max);     Serial.print(' ');
    Serial.print(t->jitter_max);   Serial.print(' ');
    Serial.println(t->fin1_ms);
    linea++;
  } else if (ahora() >= prox_rep) {
    prox_rep += MS_REPORTE;
    Serial.print(F("--- t=")); Serial.println(ahora());
    linea = 0;
  }
}

/* ---------------------------- arranque ---------------------------------- */
void setup()
{
  Serial.begin(9600);

  DDRB  |=  (uint8_t)(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));
  PORTB &= (uint8_t)~(_BV(PB0) | _BV(PB1) | _BV(PB2) | _BV(PB3));

  Serial.print(F("unidad="));   Serial.print((uint16_t)MS_POR_UNIDAD);
  Serial.print(F("ms cuanto=")); Serial.print((uint16_t)QUANTUM_MS);
#if USAR_ROUND_ROBIN
  Serial.println(F("ms politica=RR"));
#else
  Serial.println(F("ms politica=FIFO"));
#endif

  for (uint8_t i = 0; i < N_TAREAS; i++) {
    tareas[i].prox_ms = 0;        /* activacion simultanea en t = 0 */
    BaseType_t ok = xTaskCreate(vPeriodica, tareas[i].nombre, PILA_PERIODICA,
                                (void *)&tareas[i], PRIO_PERIODICAS,
                                &tareas[i].h);
    Serial.print(F("crear ")); Serial.print(tareas[i].nombre);
    Serial.print(F(" -> "));   Serial.println((int)ok);
  }

  Serial.print(F("RAM libre=")); Serial.println(ram_libre());
  Serial.println(F("tarea activ perdidos Rmax jitter fin1"));
}

/* ---------------- hooks de error de FreeRTOS ---------------------------
 * Los de la biblioteca son `weak` y solo parpadean un LED en un lazo
 * infinito, asi que un desborde de pila se ve como un cuelgue mudo.      */

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

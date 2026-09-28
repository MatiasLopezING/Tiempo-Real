/*===========================================================================
 *  Sistemas de Tiempo Real - Practica 2 - Ejercicio 4
 *
 *  Tres tareas de IGUAL prioridad que imprimen en la terminal siguiendo una
 *  secuencia fija. El orden lo impone una cadena de semaforos binarios
 *  (paso de testigo), NO el planificador.
 *
 *      A:  Tarea 1 - Tarea 3 - Tarea 2
 *      B:  Tarea 2 - Tarea 2 - Tarea 3 - Tarea 1
 *      C:  Tarea 3 - Tarea 3 - Tarea 3 - Tarea 1 - Tarea 2
 *
 *  Target  : ATmega328P @ 16 MHz (CKDIV8 sin programar), Proteus 8
 *  Serie   : USART0 -> PD0 (RXD) / PD1 (TXD), 9600 8N1 -> Virtual Terminal
 *  Libreria: Arduino_FreeRTOS (feilipu)
 *
 *  ---------------------------------------------------------------------
 *  BASE DE TIEMPO  (leer antes de medir)
 *  ---------------------------------------------------------------------
 *  Esta libreria NO usa el cristal de 16 MHz para el tick del RTOS: usa el
 *  Watchdog Timer, que corre con el oscilador RC interno de 128 kHz.
 *
 *      portUSE_WDTO       = WDTO_15MS
 *      portTICK_PERIOD_MS = 2^(0+4)         =  16 ms   <- tick real
 *      configTICK_RATE_HZ = 128000 >> 11    =  62 Hz   <- truncado de 62.5
 *
 *  Consecuencia practica: pdMS_TO_TICKS() miente por truncamiento entero.
 *
 *      pdMS_TO_TICKS(200) = (200 * 62) / 1000 = 12 ticks = 192 ms
 *
 *  O sea que pedir 200 ms da 192 ms. Por eso aca los retardos se expresan
 *  directamente en TICKS: el valor es exacto y la cuenta cierra.
 *
 *  Ademas el RC de 128 kHz tiene tolerancia propia y deriva con tension y
 *  temperatura, cosa que el cristal no. Por eso el programa imprime dos
 *  relojes en paralelo (ver IMPRIMIR_TIEMPOS):
 *
 *      xTaskGetTickCount()  -> tiempo del RTOS   (watchdog, 128 kHz RC)
 *      micros()             -> tiempo de TIMER0  (cristal, 16 MHz)
 *
 *  Comparar uno contra otro es la medicion mas interesante del practico.
 *===========================================================================*/

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <task.h>

/*======================= PARAMETROS DEL ENSAYO =============================*/

/* Secuencia a ejecutar: 'A', 'B' o 'C'. Es lo unico que hay que cambiar
   entre un caso y otro. (El #ifndef permite fijarla tambien desde la linea
   de comandos con -DSECUENCIA="'B'"; en el IDE se edita aca y listo.) */
#ifndef SECUENCIA
  #define SECUENCIA        'A'
#endif

/* Retardo de cada paso, EN TICKS (1 tick = 16 ms).
   12 ticks = 192 ms. Ver la nota de base de tiempo en el encabezado. */
#define PASO_TICKS         12

/* Todas las tareas con la misma prioridad, como pide el enunciado.
   configMAX_PRIORITIES = 4, asi que el rango valido es 0..3 y la 0 es idle. */
#define PRIO_TAREAS        1

/* En AVR el stack de tarea se expresa en BYTES.
   configMINIMAL_STACK_SIZE = 192. Serial.print() de enteros usa buffer en
   stack, asi que no conviene bajar mucho de este valor. */
#define STACK_TAREA        192

/* 1: agrega a cada linea el tick del RTOS y el tiempo medido con micros().
   0: imprime solo "Tarea N", que es lo que pide la consigna para la captura. */
#define IMPRIMIR_TIEMPOS   1

/* 1: marca en PB1/PB2/PB3 (D9/D10/D11) que tarea tiene el testigo.
   Sirve para medir con el analizador logico de Proteus, que es bastante mas
   preciso que ir parando con breakpoints. */
#define MARCAR_PINES       1

/* 1: al cerrar cada ciclo imprime el tiempo total y el stack libre minimo. */
#define DIAGNOSTICO        1

/*======================= DEFINICION DE LA SECUENCIA ========================*/

#if   SECUENCIA == 'A'
  static const uint8_t seq[] = { 1, 3, 2 };
#elif SECUENCIA == 'B'
  static const uint8_t seq[] = { 2, 2, 3, 1 };
#elif SECUENCIA == 'C'
  static const uint8_t seq[] = { 3, 3, 3, 1, 2 };
#else
  #error "SECUENCIA debe ser 'A', 'B' o 'C'"
#endif

static const uint8_t SEQ_LEN = sizeof(seq) / sizeof(seq[0]);

/*
 * Tiempo teorico de un ciclo completo:
 *      T_ciclo = SEQ_LEN * PASO_TICKS * 16 ms
 *
 *      A (3 pasos) -> 3 * 192 ms = 576 ms
 *      B (4 pasos) -> 4 * 192 ms = 768 ms
 *      C (5 pasos) -> 5 * 192 ms = 960 ms
 *
 * Es el valor contra el que hay que contrastar lo medido.
 */

/*============================== ESTADO GLOBAL ==============================*/

/* sem[0] no se usa: los indices van 1..3 para que coincidan con el numero de
   tarea y con el bit de PORTB. Cada tarea espera el suyo. */
static SemaphoreHandle_t sem[4];

/* Posicion actual dentro de seq[]. NO necesita proteccion: por construccion
   solo la toca la tarea que tiene el testigo, y hay un unico testigo. Se
   declara volatile para poder leerla desde el debugger sin que el
   compilador la mantenga en un registro. */
static volatile uint8_t idx = 0;

/* Marcas de tiempo del paso anterior, para calcular el delta. */
static volatile TickType_t tPrev = 0;
static volatile uint32_t   uPrev = 0;

/* Marcas del inicio de ciclo, para el tiempo total. */
static volatile uint32_t   uCicloIni = 0;

/*============================== TAREAS =====================================*/

static void vTarea(void *pvParameters)
{
  const uint8_t yo = (uint8_t)(uint16_t)pvParameters;   /* 1, 2 o 3 */

  for (;;)
  {
    /* ---- (1) espero mi turno -------------------------------------------
       Bloqueo indefinido. Mientras tanto la tarea esta en Blocked y no
       consume CPU. Como hay un solo testigo circulando, solo una de las
       tres tareas puede pasar de aca a la vez: ese es el mecanismo que
       garantiza el orden, sin depender de las prioridades.
       >>> BREAKPOINT UTIL: la linea de abajo se alcanza una vez por turno. */
    xSemaphoreTake(sem[yo], portMAX_DELAY);

#if MARCAR_PINES
    /* Marca alta = esta tarea TIENE EL TESTIGO. Ojo: no significa que este
       usando la CPU, porque abajo se bloquea en vTaskDelay(). El hueco entre
       el flanco de bajada de una tarea y el de subida de la siguiente es la
       latencia de handoff: give + cambio de contexto + take. */
    PORTB |= (uint8_t)(1u << yo);
#endif

    /* ---- (2) sello de tiempo con los dos relojes ------------------------ */
    const TickType_t tAhora = xTaskGetTickCount();   /* watchdog, 128 kHz RC */
    const uint32_t   uAhora = micros();              /* TIMER0, cristal 16 MHz */

    /* ---- (3) salida por la terminal -------------------------------------
       Serial.print() es bufferada e interrumpida: escribe en el buffer de TX
       (64 bytes) y vuelve enseguida, no espera a que salga el ultimo bit.
       A 9600 8N1 cada caracter tarda 10/9600 = 1,0417 ms, asi que una linea
       de ~40 caracteres ocupa la UART unos 42 ms. Entra comodo dentro de los
       192 ms del turno y el buffer nunca se llena.
       Si se bajara PASO_TICKS por debajo de ~3 ticks (48 ms) el buffer SI se
       llenaria y Serial.print() pasaria a ser bloqueante, contaminando la
       medicion. Es un limite que conviene mencionar en el informe. */
    Serial.print(F("Tarea "));
    Serial.print(yo);

#if IMPRIMIR_TIEMPOS
    Serial.print(F("  tick="));
    Serial.print((uint32_t)tAhora);
    Serial.print(F("  dt="));
    Serial.print((uint32_t)(tAhora - tPrev));       /* en ticks */
    Serial.print(F("t  real="));
    Serial.print(uAhora - uPrev);                   /* en us, por cristal */
    Serial.print(F("us"));
#endif
    Serial.println();

    tPrev = tAhora;
    uPrev = uAhora;

    /* ---- (4) cierre de ciclo -------------------------------------------- */
    if (idx == (uint8_t)(SEQ_LEN - 1))
    {
#if DIAGNOSTICO
      Serial.print(F("-- ciclo="));
      Serial.print((uAhora - uCicloIni) / 1000UL);  /* ms medidos por cristal */
      Serial.print(F("ms stack="));
      Serial.print((uint16_t)uxTaskGetStackHighWaterMark(NULL));
      Serial.println();
#endif
      uCicloIni = uAhora;
    }

    /* ---- (5) retardo del paso ------------------------------------------
       El retardo va ANTES de pasar el testigo, no despues. Si fuera despues,
       la tarea siguiente arrancaria de inmediato y los mensajes saldrian
       todos juntos: el retardo solo separaria a cada tarea de si misma, no a
       un paso del siguiente.
       vTaskDelay() es relativo y cuantiza al tick, asi que el error por paso
       es como maximo 1 tick (16 ms). Para una cadena periodica larga
       convendria xTaskDelayUntil(), que no acumula deriva. */
    vTaskDelay((TickType_t)PASO_TICKS);

#if MARCAR_PINES
    PORTB &= (uint8_t)~(1u << yo);
#endif

    /* ---- (6) paso el testigo a quien corresponda ------------------------
       Aca esta el nucleo del ejercicio: el proximo no es "el de al lado"
       sino el que dice la tabla. Por eso la misma funcion sirve para las
       tres secuencias, incluidas las que repiten una tarea (B y C): si
       seq[idx] vuelve a ser yo, la tarea se da el semaforo a si misma y en
       la vuelta siguiente lo toma sin bloquearse.
       >>> BREAKPOINT UTIL: inspeccionar idx y seq[idx] en esta linea. */
    idx = (uint8_t)((idx + 1) % SEQ_LEN);
    xSemaphoreGive(sem[ seq[idx] ]);
  }
}

/*============================== ARRANQUE ===================================*/

void setup()
{
  /* 9600 baud con F_CPU = 16 MHz -> UBRR = 103, error de baudios ~0,2 % */
  Serial.begin(9600);

#if MARCAR_PINES
  DDRB  |=  (uint8_t)(_BV(DDB1) | _BV(DDB2) | _BV(DDB3));   /* D9, D10, D11 */
  PORTB &= (uint8_t)~(_BV(PORTB1) | _BV(PORTB2) | _BV(PORTB3));
#endif

  Serial.print(F("== Ejercicio 4 - secuencia "));
  Serial.print((char)SECUENCIA);
  Serial.print(F(" - tick="));
  Serial.print((uint32_t)portTICK_PERIOD_MS);
  Serial.println(F("ms =="));

  /* Semaforos BINARIOS, no mutex: aca una tarea toma y OTRA libera, que es
     el uso por eventos. Un mutex no serviria porque tiene dueno y lo tiene
     que devolver el mismo que lo tomo.
     Nacen vacios, asi que las tres tareas quedan bloqueadas al arrancar. */
  for (uint8_t i = 1; i <= 3; i++)
  {
    sem[i] = xSemaphoreCreateBinary();
    if (sem[i] == NULL)
    {
      Serial.println(F("ERROR: sin heap para los semaforos"));
      for (;;) { }
    }
  }

  /* Las tres con la MISMA prioridad. Una sola funcion para las tres: lo
     unico que las distingue es el numero que reciben por pvParameters. */
  xTaskCreate(vTarea, "T1", STACK_TAREA, (void *)1, PRIO_TAREAS, NULL);
  xTaskCreate(vTarea, "T2", STACK_TAREA, (void *)2, PRIO_TAREAS, NULL);
  xTaskCreate(vTarea, "T3", STACK_TAREA, (void *)3, PRIO_TAREAS, NULL);

  /* Arranque de la cadena: le doy el testigo al primero de la secuencia.
     Sin este give las tres tareas se quedan bloqueadas para siempre. */
  idx       = 0;
  tPrev     = xTaskGetTickCount();
  uPrev     = micros();
  uCicloIni = uPrev;

  xSemaphoreGive(sem[ seq[0] ]);

  /* No se llama a vTaskStartScheduler(): la libreria lo hace al salir de
     setup(), desde initVariant(). */
}

void loop()
{
  /* Se ejecuta dentro de la tarea Idle (prioridad 0). Debe quedar vacia:
     cualquier cosa que se bloquee aca cuelga el sistema. */
}

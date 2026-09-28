/*
 * Sistemas de Tiempo Real - Practica 2 - Ejercicio 4
 *
 * Tres tareas de igual prioridad que imprimen en la terminal siguiendo una
 * secuencia fija. El orden lo impone una cadena de semaforos binarios (paso
 * de testigo), no el planificador.
 *
 *     A:  Tarea 1 - Tarea 3 - Tarea 2
 *     B:  Tarea 2 - Tarea 2 - Tarea 3 - Tarea 1
 *     C:  Tarea 3 - Tarea 3 - Tarea 3 - Tarea 1 - Tarea 2
 *
 * Target  : ATmega328P @ 16 MHz (CKDIV8 sin programar), Proteus 8
 * Serie   : USART0 -> PD0 (RXD) / PD1 (TXD), 9600 8N1 -> Virtual Terminal
 * Libreria: Arduino_FreeRTOS (feilipu)
 *
 * El tick del RTOS es de 16 ms y lo genera el Watchdog, no el cristal. Por eso
 * los retardos se expresan aca en ticks y no en milisegundos.
 * El analisis y las mediciones estan en Ejercicio4-Justificacion.pdf.
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <task.h>

/* ======================== PARAMETROS DEL ENSAYO ======================== */

/* Secuencia a ejecutar: 'A', 'B' o 'C'. */
#ifndef SECUENCIA
  #define SECUENCIA        'A'
#endif

#define PASO_TICKS         12    /* Retardo de cada paso. 12 ticks = 192 ms. */
#define PRIO_TAREAS        1     /* Las tres iguales, como pide el enunciado. */
#define STACK_TAREA        192   /* En AVR el stack de tarea se mide en bytes. */

#define IMPRIMIR_TIEMPOS   1     /* 1: agrega tick y microsegundos a cada linea.
                                    0: solo "Tarea N", para la captura. */
#define MARCAR_PINES       1     /* Marca en PB1/PB2/PB3 quien tiene el testigo. */
#define DIAGNOSTICO        1     /* Resumen de tiempo y stack al cerrar el ciclo. */

/* ====================== DEFINICION DE LA SECUENCIA ===================== */

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

/* ============================ ESTADO GLOBAL ============================ */

/* Un semaforo binario por tarea. sem[0] no se usa: los indices van 1..3 para
   que coincidan con el numero de tarea y con el bit de PORTB. */
static SemaphoreHandle_t sem[4];

/* Posicion dentro de seq[]. No necesita proteccion: solo la toca la tarea que
   tiene el testigo, y hay un unico testigo. volatile para poder leerla desde
   el debugger. */
static volatile uint8_t idx = 0;

/* Marcas de tiempo del paso anterior y del inicio de ciclo. */
static volatile TickType_t tPrev     = 0;
static volatile uint32_t   uPrev     = 0;
static volatile uint32_t   uCicloIni = 0;

/* =============================== TAREAS ================================ */

static void vTarea(void *pvParameters)
{
  const uint8_t yo = (uint8_t)(uint16_t)pvParameters;   /* 1, 2 o 3 */

  for (;;)
  {
    /* Espero mi turno. Bloqueo indefinido: la tarea queda en Blocked y no
       consume CPU hasta que otra le pase el testigo.
       >>> BREAKPOINT: se alcanza una vez por turno. */
    xSemaphoreTake(sem[yo], portMAX_DELAY);

#if MARCAR_PINES
    /* Marca alta = esta tarea tiene el testigo (no necesariamente la CPU). */
    PORTB |= (uint8_t)(1u << yo);
#endif

    /* Dos relojes independientes: el del RTOS (watchdog) y el del cristal. */
    const TickType_t tAhora = xTaskGetTickCount();
    const uint32_t   uAhora = micros();

    Serial.print(F("Tarea "));
    Serial.print(yo);

#if IMPRIMIR_TIEMPOS
    Serial.print(F("  tick="));
    Serial.print((uint32_t)tAhora);
    Serial.print(F("  dt="));
    Serial.print((uint32_t)(tAhora - tPrev));
    Serial.print(F("t  real="));
    Serial.print(uAhora - uPrev);
    Serial.print(F("us"));
#endif
    Serial.println();

    tPrev = tAhora;
    uPrev = uAhora;

    /* Cierre de ciclo: tiempo total medido y stack libre minimo. */
    if (idx == (uint8_t)(SEQ_LEN - 1))
    {
#if DIAGNOSTICO
      Serial.print(F("-- ciclo="));
      Serial.print((uAhora - uCicloIni) / 1000UL);
      Serial.print(F("ms stack="));
      Serial.print((uint16_t)uxTaskGetStackHighWaterMark(NULL));
      Serial.println();
#endif
      uCicloIni = uAhora;
    }

    /* El retardo va antes de pasar el testigo. Si fuera despues, la tarea
       siguiente arrancaria de inmediato y los mensajes saldrian juntos. */
    vTaskDelay((TickType_t)PASO_TICKS);

#if MARCAR_PINES
    PORTB &= (uint8_t)~(1u << yo);
#endif

    /* Paso el testigo al que indica la tabla, no al de al lado. Si seq[idx]
       vuelve a ser yo (secuencias B y C), me doy el semaforo a mi mismo.
       >>> BREAKPOINT: inspeccionar idx y seq[idx]. */
    idx = (uint8_t)((idx + 1) % SEQ_LEN);
    xSemaphoreGive(sem[ seq[idx] ]);
  }
}

/* =============================== ARRANQUE ============================== */

void setup()
{
  Serial.begin(9600);                 /* UBRR = 103, error de baudios ~0,2 % */

#if MARCAR_PINES
  DDRB  |=  (uint8_t)(_BV(DDB1) | _BV(DDB2) | _BV(DDB3));        /* D9..D11 */
  PORTB &= (uint8_t)~(_BV(PORTB1) | _BV(PORTB2) | _BV(PORTB3));
#endif

  Serial.print(F("== Ejercicio 4 - secuencia "));
  Serial.print((char)SECUENCIA);
  Serial.print(F(" - tick="));
  Serial.print((uint32_t)portTICK_PERIOD_MS);
  Serial.println(F("ms =="));

  /* Semaforos binarios, no mutex: aca una tarea toma y otra libera. Nacen
     vacios, asi que las tres quedan bloqueadas al arrancar. */
  for (uint8_t i = 1; i <= 3; i++)
  {
    sem[i] = xSemaphoreCreateBinary();
    if (sem[i] == NULL)
    {
      Serial.println(F("ERROR: sin heap para los semaforos"));
      for (;;) { }
    }
  }

  /* Una sola funcion para las tres tareas: lo unico que las distingue es el
     numero que reciben por pvParameters. */
  xTaskCreate(vTarea, "T1", STACK_TAREA, (void *)1, PRIO_TAREAS, NULL);
  xTaskCreate(vTarea, "T2", STACK_TAREA, (void *)2, PRIO_TAREAS, NULL);
  xTaskCreate(vTarea, "T3", STACK_TAREA, (void *)3, PRIO_TAREAS, NULL);

  idx       = 0;
  tPrev     = xTaskGetTickCount();
  uPrev     = micros();
  uCicloIni = uPrev;

  /* Arranque de la cadena. Sin este give las tres quedan bloqueadas para
     siempre. */
  xSemaphoreGive(sem[ seq[0] ]);

  /* vTaskStartScheduler() lo llama la libreria al salir de setup(). */
}

void loop()
{
  /* Corre dentro de la tarea Idle (prioridad 0). Debe quedar vacia. */
}

/*
 * Sistemas de Tiempo Real - Practica 2 - Ejercicio 4
 *
 * Tres tareas de igual prioridad que imprimen en la terminal siguiendo una
 * secuencia fija. El orden lo impone una cadena de semaforos binarios, no el
 * planificador.
 *
 *     A:  Tarea 1 - Tarea 3 - Tarea 2
 *     B:  Tarea 2 - Tarea 2 - Tarea 3 - Tarea 1
 *     C:  Tarea 3 - Tarea 3 - Tarea 3 - Tarea 1 - Tarea 2
 *
 * Target  : ATmega328P @ 16 MHz (CKDIV8 sin programar), Proteus 8
 * Serie   : USART0 -> PD0 (RXD) / PD1 (TXD), 9600 8N1 -> Virtual Terminal
 * Libreria: Arduino_FreeRTOS (feilipu)
 *
 * El analisis y la verificacion estan en Ejercicio4-Justificacion.pdf.
 */

#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>

/* Secuencia a ejecutar. Dejar descomentada una sola. */
static const uint8_t seq[] = { 1, 3, 2 };             /* A */
/* static const uint8_t seq[] = { 2, 2, 3, 1 }; */    /* B */
/* static const uint8_t seq[] = { 3, 3, 3, 1, 2 }; */ /* C */

static const uint8_t LEN = sizeof(seq);

/* Un semaforo binario por tarea. sem[0] no se usa: los indices van 1..3 para
   que coincidan con el numero de tarea. */
static SemaphoreHandle_t sem[4];

/* Posicion dentro de seq[]. volatile porque la escribe una tarea y la lee
   otra: sin esto el compilador podria mantenerla en un registro. */
static volatile uint8_t idx = 0;

static void vTarea(void *pv)
{
  const uint8_t yo = (uint8_t)(uint16_t)pv;   /* 1, 2 o 3 */

  for (;;)
  {
    /* Espero mi turno. Solo circula un testigo, asi que las otras dos tareas
       siguen bloqueadas mientras esta imprime. */
    xSemaphoreTake(sem[yo], portMAX_DELAY);

    Serial.print("Tarea ");
    Serial.println(yo);

    /* Separacion entre mensajes. 12 ticks de 16 ms = 192 ms. Va antes de
       pasar el testigo: si fuera despues, la siguiente arrancaria enseguida
       y los mensajes saldrian juntos. */
    vTaskDelay(12);

    /* Paso el testigo al que indica la tabla. Si vuelve a ser yo (secuencias
       B y C), me doy el semaforo a mi mismo. */
    idx = (idx + 1) % LEN;
    xSemaphoreGive(sem[ seq[idx] ]);
  }
}

void setup()
{
  Serial.begin(9600);

  /* Semaforos binarios: aca una tarea toma y otra libera. Nacen vacios, asi
     que las tres arrancan bloqueadas. */
  for (uint8_t i = 1; i <= 3; i++)
    sem[i] = xSemaphoreCreateBinary();

  /* Las tres con la misma prioridad. Una sola funcion para las tres: lo unico
     que las distingue es el numero que reciben. */
  xTaskCreate(vTarea, "T1", 128, (void *)1, 1, NULL);
  xTaskCreate(vTarea, "T2", 128, (void *)2, 1, NULL);
  xTaskCreate(vTarea, "T3", 128, (void *)3, 1, NULL);

  /* Arranque de la cadena. Sin este give las tres quedan bloqueadas. */
  xSemaphoreGive(sem[ seq[0] ]);

  /* vTaskStartScheduler() lo llama la libreria al salir de setup(). */
}

void loop()
{
  /* Corre dentro de la tarea Idle (prioridad 0). Queda vacia. */
}

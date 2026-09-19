/*
 * Ejercicio 2 - Planificación con 3 tareas compitiendo por la terminal
 *
 * Target : ATmega328P @ 16 MHz (CKDIV8 sin programar), Proteus 8
 * Serie  : USART0 -> PD0 (RXD) / PD1 (TXD), 9600 8N1 -> Virtual Terminal
 * Libreria: "FreeRTOS" (Richard Barry / feilipu) desde Library Manager
 * Placa   : Arduino Uno en Arduino IDE (solo para compilar; se simula en Proteus)
 *
 * Nota: la carpeta del sketch debe llamarse igual que el .ino.
 * Nota: el scheduler arranca solo al terminar setup(); loop() pasa a ser la
 *       idle task (prioridad 0). Por eso las tareas usan prioridad >= 1.
 */

#include <Arduino_FreeRTOS.h>
#include <semphr.h>

/* ==================== PARAMETROS DEL EXPERIMENTO ==================== */

/* Prioridades: mayor numero = mayor prioridad. Usar valores 1..3. */
#define PRIO_A      3
#define PRIO_B      2
#define PRIO_C      1

/* 0: la tarea imprime en un bucle sin bloquearse nunca (compite por CPU)
 * 1: la tarea imprime y luego se bloquea DELAY_MS con vTaskDelay          */
#define USE_DELAY   0

/* 0: acceso libre a Serial (puede mezclar salidas al haber preemption)
 * 1: mutex alrededor de Serial (cada linea sale completa)                 */
#define USE_MUTEX   0

#define DELAY_MS    200   /* Resolucion real: multiplo del tick (~15 ms) */
#define STACK_SZ    128   /* En AVR el stack se expresa en BYTES         */

/* ==================================================================== */

#if USE_MUTEX
static SemaphoreHandle_t xSerialMutex;
#endif

/* Nombres en SRAM (const char[] en AVR va a .data). Son cortos: 8 B c/u. */
static const char nameA[] = "Tarea_A";
static const char nameB[] = "Tarea_B";
static const char nameC[] = "Tarea_C";

/*
 * Cuerpo comun de las 3 tareas. El nombre llega por pvParameters, asi una
 * sola funcion sirve para las 3 y el unico factor diferencial es la
 * prioridad con la que se crea cada instancia.
 */
static void vTaskPrint(void *pvParameters)
{
  const char *name = (const char *)pvParameters;

  for (;;)
  {
#if USE_MUTEX
    /* Bloqueo indefinido hasta obtener el mutex. Con priority inheritance,
       si una tarea de baja prioridad lo tiene y una de alta lo espera,
       la de baja hereda temporalmente la prioridad alta. */
    xSemaphoreTake(xSerialMutex, portMAX_DELAY);
#endif

    /* println -> nombre + "\r\n". Sin mutex esto NO es atomico: el scheduler
       puede desalojar a la tarea entre el nombre y el salto de linea, o
       incluso en medio del nombre si el buffer TX esta lleno. */
    Serial.println(name);

#if USE_MUTEX
    xSemaphoreGive(xSerialMutex);
#endif

#if USE_DELAY
    /* La tarea pasa a estado Blocked: libera la CPU y deja correr a las de
       menor prioridad. Vuelve a Ready cuando vence el delay. */
    vTaskDelay(pdMS_TO_TICKS(DELAY_MS));
#endif
  }
}

void setup()
{
  /* 9600 baud con F_CPU = 16 MHz -> UBRR = 103 (error ~0.2 %) */
  Serial.begin(9600);

#if USE_MUTEX
  xSerialMutex = xSemaphoreCreateMutex();   /* NULL si no hay heap */
#endif

  /* xTaskCreate(funcion, nombre_debug, stack_bytes, parametro, prioridad, handle) */
  xTaskCreate(vTaskPrint, "A", STACK_SZ, (void *)nameA, PRIO_A, NULL);
  xTaskCreate(vTaskPrint, "B", STACK_SZ, (void *)nameB, PRIO_B, NULL);
  xTaskCreate(vTaskPrint, "C", STACK_SZ, (void *)nameC, PRIO_C, NULL);

  /* No se llama a vTaskStartScheduler(): la libreria lo hace al salir de setup(). */
}

void loop()
{
  /* Idle task (prioridad 0). Debe quedar vacia. Solo corre si todas las
     tareas estan Blocked (escenarios con USE_DELAY = 1). */
}

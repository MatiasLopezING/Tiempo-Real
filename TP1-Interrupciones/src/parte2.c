/*
 * tp1SistemasTR_Ej2.c
 * 
 * Ejercicio 2: Temporización por Interrupciones y Hardware (Timer0)
 */

#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>

typedef enum { INICIAL_ENCENDIDO, PARPADEANDO } modo_t;

// Variables compartidas con la ISR (declaradas volatile)
static volatile modo_t modo = INICIAL_ENCENDIDO;
static volatile uint16_t ticks_ms = 0;
static volatile uint8_t toggle = 0;

void iniciar(void) {
    // Entradas: PD2 y PD3 (0) con resistencias pull-up internas (1)
    DDRD &= ~((1 << PORTD2) | (1 << PORTD3));
    PORTD |= (1 << PORTD2) | (1 << PORTD3);

    // Salidas: PB0 y PB1 (1)
    DDRB |= (1 << PORTB0) | (1 << PORTB1);

    // Consigna a): Al iniciar, ambos LEDs deben permanecer totalmente encendidos
    PORTB |= (1 << PORTB0) | (1 << PORTB1);
}

void timer0_init(void) {
    // Modo CTC (Clear Timer on Compare Match): WGM01 = 1, WGM00 = 0
    TCCR0A = (1 << WGM01);

    // Prescaler 64: CS01 = 1, CS00 = 1 (TCCR0B)
    // Frecuencia del timer: 16 MHz / 64 = 250 kHz -> 1 tick = 4 us
    TCCR0B = (1 << CS01) | (1 << CS00);

    // Con OCR0A = 249, cuenta 250 ticks (de 0 a 249): 250 * 4 us = 1 ms exacto
    OCR0A = 249;

    // Habilitar la interrupción por comparación con OCR0A (OCIE0A)
    TIMSK0 |= (1 << OCIE0A);
}

// Rutina de Servicio de Interrupción del Timer0 (se ejecuta cada 1 ms)
ISR(TIMER0_COMPA_vect) {
    if (modo == PARPADEANDO) {
        ticks_ms++;

        // Cada 250 ms conmutamos el estado de los LEDs
        if (ticks_ms >= 250) {
            ticks_ms = 0;

            if (toggle) {
                PORTB |= (1 << PORTB0);   // PB0 Encendido
                PORTB &= ~(1 << PORTB1);  // PB1 Apagado
                toggle = 0;
            } else {
                PORTB &= ~(1 << PORTB0);  // PB0 Apagado
                PORTB |= (1 << PORTB1);   // PB1 Encendido
                toggle = 1;
            }
        }
    }
}

int main(void) {
    iniciar();
    timer0_init();

    // Habilitar interrupciones globales
    sei();

    uint8_t anterior = 0;

    while (1) {
        // Lectura de pulsadores en tiempo real (nivel bajo = presionado)
        uint8_t bt1 = !(PIND & (1 << PIND2));
        uint8_t bt2 = !(PIND & (1 << PIND3));

        if (bt1 || bt2) {
            if (anterior == 0) {
                // Al detectar la pulsación, pasamos a modo parpadeo
                if (modo == INICIAL_ENCENDIDO) {
                    modo = PARPADEANDO;
                    ticks_ms = 0;
                    toggle = 1; // Arranca con el primer estado del parpadeo

                    // PB0 ON, PB1 OFF inmediatamente
                    PORTB |= (1 << PORTB0);
                    PORTB &= ~(1 << PORTB1);
                }
                anterior = 1; // Antirrebote por flanco
            }
        } else {
            anterior = 0; // Se soltaron los pulsadores
        }
    }

    return 0;
}
/*
 * tp1SistemasTR.c
 * Ejercicio 1: Control E/S digital por polling y retardos
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

#define MASK_BOTONES ((1 << PIND2) | (1 << PIND3))

void iniciar(void) {
	DDRD &= ~((1 << PORTD2) | (1 << PORTD3)); // PD2, PD3 como entradas
	PORTD |= (1 << PORTD2) | (1 << PORTD3);   // Pull-up internas activadas
	DDRB |= (1 << PORTB0) | (1 << PORTB1);    // PB0, PB1 como salidas
}

int main(void)
{
	iniciar();

	// a) Al iniciar, ambos LEDs encendidos
	PORTB |= (1 << PORTB0) | (1 << PORTB1);

	// b) Polling de PIND: se espera hasta que se presione cualquier pulsador
	//    (con pull-up, presionado = 0)
	while ((PIND & MASK_BOTONES) == MASK_BOTONES) {
		// LEDs fijos encendidos, esperando pulsador
	}

	// b) y c) Parpadeo alternado, 250 ms por estado
	while (1)
	{
		PORTB |=  (1 << PORTB0);   // PB0 encendido
		PORTB &= ~(1 << PORTB1);   // PB1 apagado
		_delay_ms(250);

		PORTB &= ~(1 << PORTB0);   // PB0 apagado
		PORTB |=  (1 << PORTB1);   // PB1 encendido
		_delay_ms(250);
	}
}
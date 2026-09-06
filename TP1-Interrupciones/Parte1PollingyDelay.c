/*
 * tp1SistemasTR.c
 *
 * Created: 6/9/2026 16:08:58
 * Author : Juan
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

typedef enum {ENCENDIDO, APAGADO} estado_t;

void iniciar(void) {
	DDRD &= ~((1 << PORTD2) | (1 << PORTD3)); //Entradas (0)
	DDRB |= (1 << PORTB0) | (1 << PORTB1); //Salidas (1)
	PORTD |= (1 << PORTD2) | (1 << PORTD3); //Resistencias pull-up
}

int main(void)
{
    iniciar();
	estado_t estado = APAGADO;
	uint8_t anterior = 0; //Encendido(1) : Apagado(0)
	uint8_t toggle = 0; //Para el parpadeo
    while (1) 
    {
		uint8_t bt1 = !(PIND & (1 << PIND2)); //Leemos un 0 (presiono btn) 
		uint8_t bt2 = !(PIND & (1 << PIND3));
		if (bt1 || bt2) {
			if (anterior == 0) {
			    estado = (estado == ENCENDIDO) ? APAGADO : ENCENDIDO;
				anterior = 1;
			} 
		else {
			//No se registro btn presionado, reseteo		
			anterior = 0;
		}
		if (estado == ENCENDIDO) {
			if (toggle) {
				PORTB |= (1 << PORTB0);   // Encendemos LED en PB0
				PORTB &= ~(1 << PORTB1);   // Apagamos LED en PB1
				toggle = 0; 
			} else {
				PORTB &= ~(1 << PORTB0);   // Apagamos LED en PB0
				PORTB |= (1 << PORTB1);   // Encendemos LED en PB1
				toggle = 1;
			}
		} else {
			PORTB &= ~((1 << PORTB0) | (1 << PORTB1)); //Apagamos ambos leds
		}
		_delay_ms(250);
	}
}


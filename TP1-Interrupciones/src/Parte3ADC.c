#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

void adc_init(void) {
    // 1. Tensión de referencia: AVCC con capacitor externo en AREF (REFS0 = 1, REFS1 = 0)
    // 2. Canal: ADC0 (MUX3:0 = 0000)
    // 3. Justificación a la derecha (ADLAR = 0)
    ADMUX = (1 << REFS0);

    // Prescaler del ADC: F_CPU / 128 = 16 MHz / 128 = 125 kHz
    // Está dentro del rango recomendado de 50 kHz - 200 kHz (ADPS2:0 = 111)
    // ADEN = 1: Habilita el conversor ADC
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

uint16_t adc_read(void) {
    // Iniciar conversión
    ADCSRA |= (1 << ADSC);

    // Polling: Esperar hasta que la bandera ADIF se ponga en 1 (fin de conversión)
    while (!(ADCSRA & (1 << ADIF)));

    // Limpiar bandera ADIF escribiendo un '1' lógico sobre ella
    ADCSRA |= (1 << ADIF);

    // Retornar valor de 10 bits (ADCL debe leerse antes que ADCH; ADC lee ambos automáticamente)
    return ADC;
}

void io_init(void) {
    // Puerto B completo como salidas (Bus de datos D0-D7 hacia los latches)
    DDRB = 0xFF;
    PORTB = 0x00;

    // Configuración de Puerto C:
    // PC0: Entrada analógica (DDRC0 = 0, sin pull-up PORTC0 = 0)
    // PC1: Salida (LE para U4 - dígitos altos)
    // PC2: Salida (LE para U3 - dígitos bajos)
    DDRC |= (1 << DDC1) | (1 << DDC2);
    PORTC &= ~((1 << PORTC1) | (1 << PORTC2)); // Iniciar en nivel bajo
}

void display_hex(uint16_t valor) {
    // valor de 10 bits: 0x000 a 0x03FF
    uint8_t byte_alto = (uint8_t)((valor >> 8) & 0x0F); // Contiene el 0x03 (MSB)
    uint8_t byte_bajo = (uint8_t)(valor & 0xFF);        // Contiene el 0xFF (LSB)

    // --- Latch U3 (PC2) -> Displays de la IZQUIERDA (Dígitos altos) ---
    PORTB = byte_alto;
    PORTC |= (1 << PORTC2);   // LE U3 en HIGH
    _delay_us(5);
    PORTC &= ~(1 << PORTC2);  // LE U3 en LOW (retiene 03)

    // --- Latch U4 (PC1) -> Displays de la DERECHA (Dígitos bajos) ---
    PORTB = byte_bajo;
    PORTC |= (1 << PORTC1);   // LE U4 en HIGH
    _delay_us(5);
    PORTC &= ~(1 << PORTC1);  // LE U4 en LOW (retiene FF)
}
int main(void) {
    io_init();
    adc_init();

    while (1) {
        uint16_t valor_adc = adc_read();
        display_hex(valor_adc);
    }

    return 0;
}
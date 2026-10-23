/* UART command adapter: own C program, built for either ATmega168P or ATmega328P.
 * 9600 baud, 8N1. '1'/'0' set PB0. Every byte echoes from an RX interrupt. */
#include <avr/io.h>
#include <avr/interrupt.h>
static volatile unsigned char received;
static volatile unsigned char initialized = 7;
static void send(unsigned char b) { while (!(UCSR0A & (1 << UDRE0))) {} UDR0 = b; }
ISR(USART_RX_vect) {
    unsigned char b = UDR0;
    received++;
    if (b == '1') PORTB |= 1;
    if (b == '0') PORTB &= ~1;
    send(b);
}
int main(void) {
    if (received || initialized != 7) __asm__ volatile("break");
    DDRB = 1;
    UBRR0 = F_CPU / 16UL / 9600UL - 1;
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    send('R'); send('E'); send('A'); send('D'); send('Y'); send('\n');
    SMCR = 1;
    sei();
    for (;;) __asm__ volatile("sleep");
}

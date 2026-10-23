/* PCBKit message scroller. Type a line into the serial console (9600 baud): it scrolls across
 * the eight digits of a MAX7219 in a seven-segment font, letters included, a dot joining the
 * character before it. The knob sets the speed. A receive interrupt collects the line and echoes
 * every byte; Enter shows it. The CPU sleeps between interrupts. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#define LINE 64

static volatile uint16_t ms;
ISR(TIMER0_COMPA_vect) { ms++; }
static uint16_t now(void) { cli(); uint16_t t = ms; sei(); return t; }

/* Received characters until Enter; `ready` says a line is complete. */
static volatile char typed[LINE];
static volatile uint8_t length, ready;
static void send(uint8_t b) { while (!(UCSR0A & (1 << UDRE0))) {} UDR0 = b; }
ISR(USART_RX_vect) {
    uint8_t b = UDR0;
    if (b == '\r') return;
    send(b);
    if (b == '\n') { if (length) ready = 1; return; }
    if (!ready && length < LINE) typed[length++] = b;
}

/* Seven segments for each printable character, from space: bit 7 DP, then A…G. */
static const uint8_t font[] = {
    0x00, 0xa0, 0x22, 0x00, 0x00, 0x00, 0x00, 0x02, 0x4e, 0x78, 0x00, 0x00, 0x80, 0x01, 0x80, 0x25, /*  !"#$%&'()*+,-./ */
    0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b, 0x00, 0x00, 0x0d, 0x09, 0x19, 0xe5, /* 0123456789:;<=>? */
    0x7d, 0x77, 0x1f, 0x4e, 0x3d, 0x4f, 0x47, 0x5e, 0x37, 0x06, 0x3c, 0x57, 0x0e, 0x54, 0x15, 0x7e, /* @ABCDEFGHIJKLMNO */
    0x67, 0x73, 0x05, 0x5b, 0x0f, 0x3e, 0x1c, 0x2a, 0x37, 0x3b, 0x6d, 0x4e, 0x13, 0x78, 0x62, 0x08, /* PQRSTUVWXYZ[\]^_ */
};
static uint8_t glyph(uint8_t c) {
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    return c >= ' ' && c < ' ' + sizeof font ? font[c - ' '] : 0x01;
}

/* The message as segments, eight blanks before and after it so it scrolls in and out. */
static uint8_t shown[8 + LINE + 8], count;
static void compose(const volatile char *text, uint8_t n) {
    count = 0;
    for (uint8_t i = 0; i < 8; i++) shown[count++] = 0;
    for (uint8_t i = 0; i < n; i++) {
        uint8_t c = text[i];
        if ((c == '.' || c == ',') && count > 8 && !(shown[count - 1] & 0x80)) shown[count - 1] |= 0x80;
        else shown[count++] = glyph(c);
    }
    for (uint8_t i = 0; i < 8; i++) shown[count++] = 0;
}

static void spi(uint8_t byte) { SPDR = byte; while (!(SPSR & (1 << SPIF))) {} (void)SPDR; }
static void reg(uint8_t r, uint8_t value) { PORTB &= ~(1 << PB2); spi(r); spi(value); PORTB |= 1 << PB2; }
static uint16_t knob(void) { ADCSRA |= 1 << ADSC; while (ADCSRA & (1 << ADSC)) {} return ADC; }
static void print(const char *s) { while (*s) send(*s++); }

int main(void) {
    DDRB = (1 << PB2) | (1 << PB3) | (1 << PB5); PORTB = 1 << PB2;
    SPCR = (1 << SPE) | (1 << MSTR);
    ADMUX = 1 << REFS0; ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    reg(15, 0); reg(9, 0); reg(11, 7); reg(10, 10); reg(12, 1);
    UBRR0 = F_CPU / 16UL / 9600UL - 1;
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    OCR0A = 249; TCCR0A = 1 << WGM01; TCCR0B = (1 << CS01) | (1 << CS00); TIMSK0 = 1 << OCIE0A;
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    static const char hello[] = "HELLO FROM PCBKIT. TYPE A MESSAGE AND PRESS ENTER.";
    compose(hello, sizeof hello - 1);
    print("Type a message and press Enter.\n");
    uint8_t at = 0;
    uint16_t last = now();
    for (;;) {
        sleep_cpu();
        if (ready) {
            compose(typed, length);
            cli(); length = 0; ready = 0; sei();
            at = 0;
        }
        /* 60…310 ms per step: the knob up is faster. */
        if ((uint16_t)(now() - last) < 60 + ((1023 - knob()) >> 2)) continue;
        last = now();
        for (uint8_t i = 0; i < 8; i++) reg(8 - i, shown[at + i]);
        if (++at + 8 > count) at = 0;
    }
}

/* PCBKit reaction game. Press to start: red, orange, then after a random wait green and "GO".
 * Press as fast as you can: the display shows your time in milliseconds on the right and your
 * best on the left. Pressing before green is a false start. Timer0 counts milliseconds; the
 * random wait comes from a shift register stirred by the moment you pressed. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

static volatile uint16_t ms;
ISR(TIMER0_COMPA_vect) { ms++; }
static uint16_t now(void) { cli(); uint16_t t = ms; sei(); return t; }

static void spi(uint8_t byte) { SPDR = byte; while (!(SPSR & (1 << SPIF))) {} (void)SPDR; }
static void reg(uint8_t r, uint8_t value) { PORTB &= ~(1 << PB2); spi(r); spi(value); PORTB |= 1 << PB2; }

/* Segments without the decoder: bit 7 DP, then A B C D E F G. */
static const uint8_t digits[10] = {0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b};
static const char letters[] = "AbdEGHLOPrStYon-";
static const uint8_t shapes[] = {0x77, 0x1f, 0x3d, 0x4f, 0x5e, 0x37, 0x0e, 0x7e, 0x67, 0x05, 0x5b, 0x0f, 0x3b, 0x1d, 0x15, 0x01};
static uint8_t glyph(char c) {
    if (c >= '0' && c <= '9') return digits[c - '0'];
    for (uint8_t i = 0; letters[i]; i++) if (letters[i] == c) return shapes[i];
    return 0;
}
/* Eight characters, left to right. */
static void text(const char *s) { for (uint8_t i = 0; i < 8; i++) reg(8 - i, glyph(s[i])); }
/* A number right aligned into `width` characters (at most 10^width - 1). */
static void number(char *out, uint8_t width, uint16_t value) {
    for (uint8_t i = width; i-- > 0;) {
        uint16_t tens = 0;
        while (value >= 10) { value -= 10; tens++; }   /* value / 10 without a library */
        out[i] = '0' + (char)value;
        value = tens;
        if (!value) { while (i-- > 0) out[i] = ' '; break; }
    }
}

/* The lab's button latches: every change of its contact counts as a press. */
static uint8_t button, seed = 0xa5;
static uint8_t pressed(void) {
    uint8_t down = PIND & (1 << PD2), edge = down != button;
    button = down;
    return edge;
}
static uint8_t random(void) { seed = (seed >> 1) ^ (-(seed & 1) & 0xb8); return seed; }
static void lights(uint8_t red, uint8_t orange, uint8_t green) { PORTD = (1 << PD2) | (red << PD5) | (orange << PD6) | (green << PD7); }
/* Wait, sleeping; a press ends the wait early. */
static uint8_t wait(uint16_t duration) {
    uint16_t start = now();
    while ((uint16_t)(now() - start) < duration) { if (pressed()) return 1; sleep_cpu(); }
    return 0;
}

int main(void) {
    DDRB = (1 << PB2) | (1 << PB3) | (1 << PB5); PORTB = 1 << PB2;
    DDRD = (1 << PD5) | (1 << PD6) | (1 << PD7); PORTD = 1 << PD2;
    SPCR = (1 << SPE) | (1 << MSTR);
    reg(15, 0); reg(9, 0); reg(11, 7); reg(10, 10); reg(12, 1);
    OCR0A = 249; TCCR0A = 1 << WGM01; TCCR0B = (1 << CS01) | (1 << CS00); TIMSK0 = 1 << OCIE0A;
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    button = PIND & (1 << PD2);
    uint16_t best = 0;
    for (;;) {
        /* Attract: the lights chase until a press. */
        text(" PrESS  ");
        for (uint8_t i = 0;; i = i == 2 ? 0 : i + 1) { lights(i == 0, i == 1, i == 2); if (wait(250)) break; }
        seed ^= (uint8_t)now();
        text(" rEAdY  ");
        lights(1, 0, 0); if (wait(700)) goto early;
        lights(1, 1, 0); if (wait(700)) goto early;
        lights(0, 0, 0); if (wait(800 + (uint16_t)random() * 8)) goto early;
        lights(0, 0, 1); text("   GO   ");
        uint16_t start = now();
        while (!pressed()) { sleep_cpu(); if ((uint16_t)(now() - start) > 9999) break; }
        uint16_t time = now() - start;
        if (!best || time < best) best = time;
        /* "b" and the best on the left, this time on the right. */
        char line[9] = "b       ";
        number(line + 1, 3, best > 999 ? 999 : best); number(line + 4, 4, time);
        lights(0, 0, time == best);
        text(line);
        wait(3000);
        continue;
    early:
        lights(1, 1, 1); text(" EArLY  ");
        wait(2000);
    }
}

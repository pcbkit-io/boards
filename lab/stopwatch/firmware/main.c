/* PCBKit stopwatch: a 1 kHz Timer0 interrupt counts in decimal digits (no division), a MAX7219
 * shows HH.MM.SS.cc. The run switch starts and stops it (green: running, red: stopped). The lap
 * button freezes the display for two seconds while the count goes on (orange); stopped, it
 * resets to zero. The CPU sleeps between interrupts. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

/* Digits from hundredths (0) to tens of hours (7), and where each one wraps. */
static volatile uint8_t digit[8];
static const uint8_t wrap[8] = {10, 10, 10, 6, 10, 6, 10, 10};
static volatile uint8_t running, sub, ticks;

ISR(TIMER0_COMPA_vect) {
    ticks++;
    if (!running || ++sub < 10) return;
    sub = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (++digit[i] < wrap[i]) break;
        digit[i] = 0;
    }
}

static void spi(uint8_t byte) { SPDR = byte; while (!(SPSR & (1 << SPIF))) {} (void)SPDR; }
static void display(uint8_t reg, uint8_t value) { PORTB &= ~(1 << PB2); spi(reg); spi(value); PORTB |= 1 << PB2; }
static void show(const uint8_t *d) {
    /* Code B: 15 is blank, bit 7 the decimal point after hours, minutes and seconds. */
    for (uint8_t i = 0; i < 7; i++) display(i + 1, d[i] | (i == 2 || i == 4 || i == 6 ? 0x80 : 0));
    display(8, d[7] ? d[7] : 15);
}
/* The lab's buttons latch: every change of a contact counts as a press. */
static uint8_t changed(uint8_t mask, uint8_t *before) {
    uint8_t now = PIND & mask, edge = now != *before;
    *before = now;
    return edge;
}

int main(void) {
    DDRB = (1 << PB2) | (1 << PB3) | (1 << PB5); PORTB = 1 << PB2;
    DDRD = (1 << PD5) | (1 << PD6) | (1 << PD7); PORTD = (1 << PD2) | (1 << PD3);
    SPCR = (1 << SPE) | (1 << MSTR);
    display(15, 0); display(9, 0xff); display(11, 7); display(10, 9); display(12, 1);
    OCR0A = 249; TCCR0A = 1 << WGM01; TCCR0B = (1 << CS01) | (1 << CS00); TIMSK0 = 1 << OCIE0A; /* 16 MHz / 64 / 250 */
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    uint8_t lap = PIND & (1 << PD3), frozen[8], hold = 0;
    for (;;) {
        sleep_cpu();
        if (ticks < 10) continue;
        ticks = 0;
        running = !(PIND & (1 << PD2));
        if (changed(1 << PD3, &lap)) {
            if (running) {
                cli(); for (uint8_t i = 0; i < 8; i++) frozen[i] = digit[i]; sei();
                hold = 200;
            } else {
                cli(); for (uint8_t i = 0; i < 8; i++) digit[i] = 0; sub = 0; sei();
                hold = 0;
            }
        }
        if (hold) { hold--; show(frozen); }
        else { uint8_t now[8]; cli(); for (uint8_t i = 0; i < 8; i++) now[i] = digit[i]; sei(); show(now); }
        PORTD = (PORTD & ((1 << PD2) | (1 << PD3))) | (running ? 1 << PD5 : 1 << PD7) | (hold ? 1 << PD6 : 0);
    }
}

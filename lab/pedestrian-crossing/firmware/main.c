/* PCBKit pedestrian crossing. Cars have green until people get their turn: every 20 s, or 3 s
 * after someone presses the request button (the display counts down either way). Then amber,
 * red, GO for 8 s with a countdown, flashing for the last 3, red-amber, and green again. One timer
 * interrupt counts milliseconds; the program is a table of phases. The CPU sleeps in between. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

static volatile uint16_t ms;
ISR(TIMER0_COMPA_vect) { ms++; }
static uint16_t now(void) { cli(); uint16_t t = ms; sei(); return t; }

static void spi(uint8_t byte) { SPDR = byte; while (!(SPSR & (1 << SPIF))) {} (void)SPDR; }
static void reg(uint8_t r, uint8_t value) { PORTB &= ~(1 << PB2); spi(r); spi(value); PORTB |= 1 << PB2; }
static const uint8_t digits[10] = {0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b};
/* Four letters on the left, a count of seconds (0…99, or none) on the right. */
static void show(const uint8_t *word, uint8_t seconds) {
    for (uint8_t i = 0; i < 4; i++) reg(8 - i, word[i]);
    uint8_t tens = 0;
    while (seconds >= 10 && seconds != 0xff) { seconds -= 10; tens++; }
    reg(4, 0); reg(3, 0);
    reg(2, seconds == 0xff || !tens ? 0 : digits[tens]);
    reg(1, seconds == 0xff ? 0 : digits[seconds]);
}
static const uint8_t PUSH[4] = {0x67, 0x3e, 0x5b, 0x37}, WAIT[4] = {0x01, 0x01, 0x01, 0x01},
    GO[4] = {0x5e, 0x7e, 0, 0}, STOP[4] = {0x5b, 0x0f, 0x7e, 0x67};

/* Car lights on PD5…7 (red, amber, green), walk lights on PB0…1 (red, green). */
enum { CR = 1, CA = 2, CG = 4, WR = 8, WG = 16, FLASH = 32 };
struct phase { uint8_t lights; uint16_t ms; };
static const struct phase phases[] = {
    {CG | WR, 0},          /* green for cars until it is time: see below */
    {CA | WR, 3000},
    {CR | WR, 1500},
    {CR | WG, 5000},
    {CR | WG | FLASH, 3000},
    {CR | WR, 1500},
    {CR | CA | WR, 1500},
};
#define PHASES (sizeof phases / sizeof phases[0])

int main(void) {
    DDRB = (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB5); PORTB = 1 << PB2;
    DDRD = (1 << PD5) | (1 << PD6) | (1 << PD7); PORTD = 1 << PD2;
    SPCR = (1 << SPE) | (1 << MSTR);
    reg(15, 0); reg(9, 0); reg(11, 7); reg(10, 10); reg(12, 1);
    OCR0A = 249; TCCR0A = 1 << WGM01; TCCR0B = (1 << CS01) | (1 << CS00); TIMSK0 = 1 << OCIE0A;
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    uint8_t button = PIND & (1 << PD2), phase = 0;
    uint16_t start = now(), length = 20000;
    for (uint8_t wakes = 0;; wakes = 0) {
        while (++wakes < 20) sleep_cpu(); /* every 20 ms */
        uint16_t elapsed = now() - start;
        /* The lab's button latches: every change of its contact counts as a press. */
        uint8_t down = PIND & (1 << PD2);
        if (down != button) { button = down; if (phase == 0 && length - elapsed > 3000 && elapsed < length) length = elapsed + 3000; }
        if (elapsed >= length) {
            phase = phase + 1 == PHASES ? 0 : phase + 1;
            start = now(); elapsed = 0;
            length = phase ? phases[phase].ms : 20000;
        }
        uint8_t l = phases[phase].lights;
        uint16_t left = length - elapsed;
        if ((l & FLASH) && ((left >> 8) & 1)) l &= ~WG;
        PORTD = (1 << PD2) | ((l & CR) ? 1 << PD5 : 0) | ((l & CA) ? 1 << PD6 : 0) | ((l & CG) ? 1 << PD7 : 0);
        PORTB = (PORTB & ~3) | ((l & WR) ? 1 : 0) | ((l & WG) ? 2 : 0);
        /* Seconds left, rounded up, without a division. */
        uint8_t seconds = 0;
        while (left > 0) { seconds++; left = left > 1000 ? left - 1000 : 0; }
        if (phase == 0) show(length == 20000 ? PUSH : WAIT, seconds);
        else if (phase == 3 || phase == 4) show(GO, phase == 3 ? seconds + 3 : seconds);
        else show(STOP, 0xff);
    }
}

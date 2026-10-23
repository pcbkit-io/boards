/* PCBKit LED ring: twelve lights around the board, each dimmed by a 16-level software PWM in a
 * 10 kHz Timer0 interrupt. The button cycles the modes: a comet spinning round with a fading
 * tail (the knob sets its speed), a dial gauge of the knob from green to red, a breathing wave
 * chasing round the ring. The CPU sleeps between interrupts. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#define LIGHTS 12

static volatile uint8_t level[LIGHTS];   /* 0 (off) … 15 (full); lights 0…7 on PORTD, 8…11 on PB0…3 */
static volatile uint16_t ms;

ISR(TIMER0_COMPA_vect) {
    static uint8_t phase, tick;
    phase = (phase + 1) & 15;
    uint8_t d = 0, b = 0;
    for (uint8_t i = 0; i < 8; i++) if (level[i] > phase) d |= 1 << i;
    for (uint8_t i = 0; i < 4; i++) if (level[8 + i] > phase) b |= 1 << i;
    PORTD = d;
    PORTB = b;
    if (++tick == 10) { tick = 0; ms++; }
}

static uint16_t now(void) { cli(); uint16_t t = ms; sei(); return t; }
static void wait(uint16_t duration) { uint16_t start = now(); while ((uint16_t)(now() - start) < duration) sleep_cpu(); }
static uint16_t knob(void) { ADCSRA |= 1 << ADSC; while (ADCSRA & (1 << ADSC)) {} return ADC; }
/* The lab's button latches: every change of its contact counts as a press. */
static uint8_t pressed(void) {
    static uint8_t before = 1 << PC1;
    uint8_t now = PINC & (1 << PC1), changed = now != before;
    before = now;
    return changed;
}

int main(void) {
    DDRD = 0xff; DDRB = 0x0f; PORTC = 1 << PC1; /* PC1: the button, with its pull-up */
    ADMUX = 1 << REFS0; ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    OCR0A = 199; TCCR0A = 1 << WGM01; TCCR0B = 1 << CS01; TIMSK0 = 1 << OCIE0A; /* 16 MHz / 8 / 200 */
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    uint8_t mode = 0, at = 0, phase = 0;
    for (;;) {
        if (pressed()) { mode = mode == 2 ? 0 : mode + 1; for (uint8_t i = 0; i < LIGHTS; i++) level[i] = 0; }
        uint16_t value = knob();
        if (mode == 0) {
            /* The comet: every light fades by a third, the head is full. */
            for (uint8_t i = 0; i < LIGHTS; i++) level[i] = (level[i] * 5) >> 3;
            level[at] = 15;
            at = at == LIGHTS - 1 ? 0 : at + 1;
            wait(25 + (value >> 3));
        } else if (mode == 1) {
            /* The gauge: 12 × 16 steps over the knob's travel, the last light partly lit. */
            uint8_t total = (value * 3) >> 4, full = total >> 4;
            for (uint8_t i = 0; i < LIGHTS; i++) level[i] = i < full ? 15 : i == full ? (total & 15) : 0;
            wait(20);
        } else {
            /* The wave: two bright crests travelling round. */
            phase++;
            for (uint8_t i = 0; i < LIGHTS; i++) { uint8_t x = (phase + i * 5) & 31; level[i] = x < 16 ? x : 31 - x; }
            wait(30);
        }
    }
}

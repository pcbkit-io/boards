/* PCBKit servo sequencer. One Timer1 interrupt drives three servos in turn (a pulse each,
 * then the rest of the 20 ms frame), so any pins can carry servos. At first the servos dance
 * a built-in choreography. Press the button to teach (red): the knobs move the servos and every
 * 80 ms a pose is recorded, up to 16 s. Press again to replay it in a loop (green), smoothly
 * between the poses. The CPU sleeps between interrupts. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

#define SERVOS 3
#define POSES 200     /* 16 s at 12.5 poses per second */
#define STEPS 4       /* 20 ms frames per pose */

static volatile uint16_t pulse[SERVOS] = {3000, 3000, 3000}; /* 0.5 µs ticks: 2000…4000 */
static volatile uint8_t frames;

ISR(TIMER1_COMPA_vect) {
    static uint8_t slot;
    PORTD &= ~((1 << PD5) | (1 << PD6) | (1 << PD7));
    if (slot < SERVOS) { PORTD |= 1 << (PD5 + slot); OCR1A = pulse[slot] - 1; slot++; }
    else { OCR1A = 40000 - 1 - pulse[0] - pulse[1] - pulse[2]; slot = 0; frames++; }
}

static uint16_t ticks(uint8_t degrees) { return 2000 + ((degrees * 89) >> 3); } /* 1000…2000 µs */
static uint8_t knob(uint8_t channel) {
    ADMUX = (1 << REFS0) | channel;
    ADCSRA |= 1 << ADSC;
    while (ADCSRA & (1 << ADSC)) {}
    return (ADC * 45) >> 8; /* 0…179° */
}
static uint8_t triangle(uint8_t x) { return x < 128 ? x : 255 - x; }

static uint8_t poses[POSES][SERVOS];

int main(void) {
    DDRD = (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7); PORTD = 1 << PD2;
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    OCR1A = 999; TCCR1B = (1 << WGM12) | (1 << CS11); TIMSK1 = 1 << OCIE1A; /* CTC, 0.5 µs */
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    enum { DANCE, TEACH, REPLAY } mode = DANCE;
    uint8_t button = PIND & (1 << PD2), count = 0, at = 0, step = 0, t = 0;
    for (;;) {
        uint8_t seen = frames;
        while (frames == seen) sleep_cpu();
        /* The lab's button latches: every change of its contact counts as a press. */
        uint8_t down = PIND & (1 << PD2);
        if (down != button) {
            button = down;
            if (mode == TEACH && count) { mode = REPLAY; at = 0; step = 0; }
            else { mode = TEACH; count = 0; step = 0; }
        }
        uint8_t angle[SERVOS];
        if (mode == DANCE) {
            t++;
            angle[0] = 26 + triangle(t);                  /* slow sweep */
            angle[1] = 26 + triangle(t + t / 2 + 64);     /* half again as fast, a quarter behind */
            angle[2] = triangle(t << 2) < 64 ? 40 : 140;  /* snaps open and shut */
        } else if (mode == TEACH) {
            for (uint8_t i = 0; i < SERVOS; i++) angle[i] = knob(i);
            if (step == 0 && count < POSES) { for (uint8_t i = 0; i < SERVOS; i++) poses[count][i] = angle[i]; count++; }
            if (++step == STEPS) step = 0;
            if (count == POSES) { mode = REPLAY; at = 0; step = 0; }
        } else {
            /* Between this pose and the next, step by step. */
            uint8_t next = at + 1 == count ? 0 : at + 1;
            for (uint8_t i = 0; i < SERVOS; i++) {
                int16_t a = poses[at][i], b = poses[next][i];
                angle[i] = a + (((b - a) * step) >> 2); /* / STEPS */
            }
            if (++step == STEPS) { step = 0; at = next; }
        }
        for (uint8_t i = 0; i < SERVOS; i++) { uint16_t p = ticks(angle[i]); cli(); pulse[i] = p; sei(); }
        uint8_t blink = mode == TEACH && count > POSES - 50 && (count & 4);
        uint8_t lights = ((mode == TEACH && !blink) << PD3) | ((mode != TEACH) << PD4);
        cli(); PORTD = (PORTD & ~((1 << PD3) | (1 << PD4))) | lights; sei(); /* the interrupt writes PORTD too */
    }
}

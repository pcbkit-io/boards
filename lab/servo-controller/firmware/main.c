/* PCBKit interactive controller: real ADC interrupts, Timer0 clock, Timer1 servo PWM.
 * The button returns the servo to centre. Otherwise the potentiometer sets the angle.
 * CPU sleeps between interrupts; no Arduino/runtime object is linked. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>
static uint16_t pulse(uint16_t value) {
    uint32_t numerator=0, factor=2000, result=0, remainder=0, mask=0x80000000UL;
    do { if(value&1) numerator+=factor; value>>=1; factor<<=1; } while(value);
    do { remainder=(remainder<<1)|((numerator&mask)!=0); if(remainder>=1023){remainder-=1023;result|=mask;} mask>>=1; } while(mask);
    return 1999+(uint16_t)result;
}
ISR(TIMER0_COMPA_vect) { if (!(ADCSRA & (1<<ADSC))) ADCSRA |= 1<<ADSC; }
ISR(ADC_vect) {
    uint16_t value=ADC;
    if (!(PIND & (1<<PD2))) { OCR1A=2999; PORTD |= 1<<PD7; }
    else { OCR1A=pulse(value); PORTD &= ~(1<<PD7); }
}
int main(void) {
    DDRB=1<<PB1; DDRD=1<<PD7; PORTD=1<<PD2;
    ICR1=39999; OCR1A=2999; TCCR1A=(1<<COM1A1)|(1<<WGM11); TCCR1B=(1<<WGM13)|(1<<WGM12)|(1<<CS11);
    ADMUX=1<<REFS0; ADCSRA=(1<<ADEN)|(1<<ADIE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
    OCR0A=249; TCCR0A=1<<WGM01; TCCR0B=(1<<CS01)|(1<<CS00); TIMSK0=1<<OCIE0A;
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    for (;;) sleep_cpu();
}

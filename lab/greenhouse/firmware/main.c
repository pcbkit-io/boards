/* PCBKit greenhouse controller. An SHT31 measures temperature, an SCD40 CO2; the knob sets the
 * temperature (15…35 °C). Warmer than the setpoint opens the window 30° per degree, CO2 over
 * 1000 ppm opens it too; the servo moves it slowly. The display shows the temperature on the
 * left and CO2 on the right, or the setpoint while the knob turns. Red: too cold (a heater would
 * run), orange: window open, green: on target. Change the lab's temperature and CO2 to see it. */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdint.h>

static volatile uint16_t ms;
ISR(TIMER0_COMPA_vect) { ms++; }
static uint16_t now(void) { cli(); uint16_t t = ms; sei(); return t; }
static void wait(uint16_t duration) { uint16_t start = now(); while ((uint16_t)(now() - start) < duration) sleep_cpu(); }

/* Own integer helpers: the image links without a runtime library. */
static uint32_t quotient(uint32_t numerator, uint16_t denominator) {
    uint32_t result = 0, remainder = 0, mask = 0x80000000UL;
    do {
        remainder = (remainder << 1) | ((numerator & mask) != 0);
        if (remainder >= denominator) { remainder -= denominator; result |= mask; }
        mask >>= 1;
    } while (mask);
    return result;
}
static uint32_t scaled(uint16_t value, uint32_t factor) {
    uint32_t result = 0;
    do { if (value & 1) result += factor; factor <<= 1; value >>= 1; } while (value);
    return result;
}

/* I2C (TWI), polled. */
static uint8_t twi(uint8_t command) { TWCR = command; while (!(TWCR & (1 << TWINT))) {} return TWSR & 0xf8; }
static uint8_t start(uint8_t address) {
    uint8_t status = twi((1 << TWINT) | (1 << TWEN) | (1 << TWSTA));
    if (status != 0x08 && status != 0x10) return 0;
    TWDR = address;
    status = twi((1 << TWINT) | (1 << TWEN));
    return status == (address & 1 ? 0x40 : 0x18);
}
static void stop(void) { TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO); while (TWCR & (1 << TWSTO)) {} }
static uint8_t send(uint8_t byte) { TWDR = byte; return twi((1 << TWINT) | (1 << TWEN)) == 0x28; }
static uint8_t receive(uint8_t ack) { twi((1 << TWINT) | (1 << TWEN) | (ack ? 1 << TWEA : 0)); return TWDR; }
static uint8_t crc(const uint8_t *data) {
    uint8_t value = 0xff;
    for (uint8_t j = 0; j < 2; j++) {
        value ^= data[j];
        for (uint8_t i = 0; i < 8; i++) value = value & 0x80 ? (value << 1) ^ 0x31 : value << 1;
    }
    return value;
}
static uint8_t read(uint8_t address, uint8_t *bytes, uint8_t length) {
    uint8_t ok = start(address | 1);
    if (ok) for (uint8_t i = 0; i < length; i++) bytes[i] = receive(i + 1 != length);
    stop();
    for (uint8_t i = 0; ok && i < length; i += 3) ok = crc(bytes + i) == bytes[i + 2];
    return ok;
}
static uint8_t command(uint8_t address, uint8_t high, uint8_t low) { uint8_t ok = start(address) && send(high) && send(low); stop(); return ok; }

/* SHT31 single shot: tenths of a degree, or 0x7fff on an error. */
static int16_t temperature(void) {
    uint8_t bytes[6];
    if (!command(0x88, 0x24, 0x00)) return 0x7fff;
    wait(16);
    if (!read(0x88, bytes, 6)) return 0x7fff;
    return -450 + (int16_t)quotient(scaled(((uint16_t)bytes[0] << 8) | bytes[1], 1750) + 32767, 65535);
}
/* SCD40 periodic measurement: a new CO2 value in ppm every 5 s, else the last one. */
static uint16_t co2(uint16_t last) {
    uint8_t bytes[9];
    if (!command(0xc4, 0xe4, 0xb8)) return 0;
    wait(2); /* at least 1 ms: the counter may tick at once */
    if (!read(0xc4, bytes, 3) || (!(bytes[0] & 7) && !bytes[1])) return last;
    if (!command(0xc4, 0xec, 0x05)) return 0;
    wait(2); /* at least 1 ms: the counter may tick at once */
    if (!read(0xc4, bytes, 9)) return 0;
    return ((uint16_t)bytes[0] << 8) | bytes[1];
}

/* MAX7219 without the decoder: bit 7 DP, then segments A…G. */
static void spi(uint8_t byte) { SPDR = byte; while (!(SPSR & (1 << SPIF))) {} (void)SPDR; }
static void reg(uint8_t r, uint8_t value) { PORTB &= ~(1 << PB2); spi(r); spi(value); PORTB |= 1 << PB2; }
static const uint8_t digits[10] = {0x7e, 0x30, 0x6d, 0x79, 0x33, 0x5b, 0x5f, 0x70, 0x7f, 0x7b};
/* Four characters ending at `last` (1 = the rightmost): a value, `point` digits after the point. */
static void field(uint8_t last, int16_t value, uint8_t point) {
    uint8_t negative = value < 0;
    uint16_t v = negative ? -value : value;
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t tens = 0, rest = v;
        while (rest >= 10) { rest -= 10; tens++; }
        uint8_t shown = v || i <= point;
        reg(last + i, shown ? digits[rest] | (i == point && point ? 0x80 : 0) : (negative ? 0x01 : 0));
        if (!shown) negative = 0;
        v = tens;
    }
}
static void dashes(uint8_t last) { for (uint8_t i = 0; i < 4; i++) reg(last + i, 0x01); }

static uint16_t knob(void) { ADCSRA |= 1 << ADSC; while (ADCSRA & (1 << ADSC)) {} return ADC; }

int main(void) {
    DDRB = (1 << PB1) | (1 << PB2) | (1 << PB3) | (1 << PB5); PORTB = 1 << PB2;
    DDRD = (1 << PD5) | (1 << PD6) | (1 << PD7);
    SPCR = (1 << SPE) | (1 << MSTR);
    TWBR = 72; /* 100 kHz */
    ADMUX = 1 << REFS0; ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    reg(15, 0); reg(9, 0); reg(11, 7); reg(10, 8); reg(12, 1);
    /* Servo: 50 Hz fast PWM on OC1A, 0.5 µs steps; 1000…2000 µs is 0…180°. */
    ICR1 = 39999; OCR1A = 2000; TCCR1A = (1 << COM1A1) | (1 << WGM11); TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
    OCR0A = 249; TCCR0A = 1 << WGM01; TCCR0B = (1 << CS01) | (1 << CS00); TIMSK0 = 1 << OCIE0A;
    set_sleep_mode(SLEEP_MODE_IDLE); sleep_enable(); sei();
    wait(40); /* SCD40 power-up */
    command(0xc4, 0x21, 0xb1);
    int16_t celsius = 0x7fff;
    uint16_t ppm = 0, last = 0xffff, angle = 0, showing = 0;
    for (uint8_t tick = 0;; tick++) {
        uint16_t value = knob();
        int16_t setpoint = 150 + (int16_t)((value * 25) >> 7); /* 15.0 … 35.0 °C in tenths */
        if (last != 0xffff && (value > last ? value - last : last - value) > 2) showing = 40;
        last = value;
        if (!(tick & 7)) { celsius = temperature(); ppm = co2(ppm); }
        /* How far the window should open: 3° per tenth over the setpoint, and for CO2. */
        uint16_t target = 0;
        if (celsius != 0x7fff && celsius > setpoint) target = celsius - setpoint > 30 ? 90 : (celsius - setpoint) * 3;
        if (ppm > 1000) { uint16_t extra = ppm - 1000 > 1000 ? 90 : ((ppm - 1000) * 23) >> 8; if (extra > target) target = extra; }
        /* Move 2° per step: a window, not a flag. */
        if (angle < target) angle = target - angle > 2 ? angle + 2 : target;
        else if (angle > target) angle = angle - target > 2 ? angle - 2 : target;
        OCR1A = 2000 + ((angle * 89) >> 3);
        uint8_t cold = celsius != 0x7fff && celsius < setpoint - 5;
        PORTD = (cold << PD5) | ((angle > 0) << PD6) | ((!cold && !angle && celsius != 0x7fff) << PD7);
        if (showing) {
            showing--;
            reg(8, 0x5b); reg(7, 0x67); /* "SP" */
            field(1, setpoint, 1); reg(5, 0); reg(6, 0);
        } else {
            if (celsius == 0x7fff) dashes(5); else field(5, celsius, 1);
            if (!ppm) dashes(1); else field(1, ppm > 9999 ? 9999 : ppm, 0);
        }
        wait(50);
    }
}

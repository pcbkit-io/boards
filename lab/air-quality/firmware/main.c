/* PCBKit's first lab instrument. ATmega328P at 16 MHz, SHT31 on TWI,
 * MAX7219 on SPI, LOAD on PB2. SCD40 on TWI, CO2 LEDs on PD5/6/7. This is executable freestanding C firmware. */
#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdint.h>

/* Own integer helpers: the bundled image links without third-party runtime objects. */
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
static uint8_t twi(uint8_t command) {
    TWCR = command;
    while (!(TWCR & (1 << TWINT))) {}
    return TWSR & 0xf8;
}
static uint8_t start(uint8_t address) {
    uint8_t status = twi((1 << TWINT) | (1 << TWEN) | (1 << TWSTA));
    if (status != 0x08 && status != 0x10) return 0;
    TWDR = address;
    status = twi((1 << TWINT) | (1 << TWEN));
    return status == (address & 1 ? 0x40 : 0x18);
}
static void stop(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
    while (TWCR & (1 << TWSTO)) {}
}
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
static void spi(uint8_t byte) { SPDR = byte; while (!(SPSR & (1 << SPIF))) {} (void)SPDR; }
static void display(uint8_t reg, uint8_t value) { PORTB &= ~(1 << PB2); spi(reg); spi(value); PORTB |= 1 << PB2; }
static void number(uint8_t first, int16_t tenths) {
    uint8_t negative = tenths < 0;
    uint16_t value = negative ? -tenths : tenths;
    uint16_t tens = quotient(value, 10), hundreds = quotient(tens, 10), thousands = quotient(hundreds, 10);
    display(first, value - tens * 10);
    display(first + 1, 0x80 | (tens - hundreds * 10));
    display(first + 2, value >= 100 ? hundreds - thousands * 10 : (negative ? 10 : 15));
    display(first + 3, value >= 1000 ? thousands : (negative && value >= 100 ? 10 : 15));
}
/* Example thresholds, not a health/safety classification. Edit them for the application. */
#define CO2_ORANGE_PPM 800
#define CO2_RED_PPM 1200
static uint8_t scd_command(uint8_t high, uint8_t low) {
    uint8_t ok = start(0xc4) && send(high) && send(low); stop(); return ok;
}
static uint8_t scd_read(uint8_t *bytes, uint8_t length) {
    __builtin_avr_delay_cycles(F_CPU / 1000UL);
    uint8_t ok = start(0xc5);
    if (ok) for (uint8_t i = 0; i < length; i++) bytes[i] = receive(i + 1 != length);
    stop(); return ok;
}
static void co2_indicators(void) {
    uint8_t bytes[9];
    if (!scd_command(0xe4, 0xb8) || !scd_read(bytes, 3) || crc(bytes) != bytes[2]) { PORTD = 0; return; }
    if (!(bytes[0] & 7) && !bytes[1]) return; /* Keep the previous result until a fresh sample. */
    if (!scd_command(0xec, 0x05) || !scd_read(bytes, 9)) { PORTD = 0; return; }
    for (uint8_t i = 0; i < 9; i += 3) if (crc(bytes + i) != bytes[i + 2]) { PORTD = 0; return; }
    uint16_t ppm = ((uint16_t)bytes[0] << 8) | bytes[1];
    PORTD = ppm < CO2_ORANGE_PPM ? 1 << PD5 : ppm < CO2_RED_PPM ? 1 << PD6 : 1 << PD7;
}
int main(void) {
    DDRB = (1 << PB2) | (1 << PB3) | (1 << PB5);
    PORTB = 1 << PB2;
    DDRD = (1 << PD5) | (1 << PD6) | (1 << PD7);
    PORTD = 0;
    SPCR = (1 << SPE) | (1 << MSTR);
    TWBR = 72; /* 100 kHz TWI */
    display(12, 1); display(15, 0); display(11, 7); display(9, 255); display(10, 8);
    __builtin_avr_delay_cycles(F_CPU / 1000UL * 40); /* SCD40 power-up */
    scd_command(0x21, 0xb1); /* 5 s periodic mode; 0x21ac selects 30 s low-power mode. */
    for (;;) {
        uint8_t bytes[6];
        uint8_t ok = start(0x88) && send(0x24) && send(0x00);
        stop();
        __builtin_avr_delay_cycles(F_CPU / 1000UL * 16);
        ok = ok && start(0x89);
        if (ok) for (uint8_t i = 0; i < 6; i++) bytes[i] = receive(i != 5);
        stop();
        if (ok && crc(bytes) == bytes[2] && crc(bytes + 3) == bytes[5]) {
            uint16_t raw_t = ((uint16_t)bytes[0] << 8) | bytes[1];
            uint16_t raw_h = ((uint16_t)bytes[3] << 8) | bytes[4];
            int16_t temperature = -450 + quotient(scaled(raw_t, 1750) + 32767, 65535);
            uint16_t humidity = quotient(scaled(raw_h, 1000) + 32767, 65535);
            number(5, temperature);
            number(1, humidity);
        } else {
            for (uint8_t i = 1; i <= 8; i++) display(i, 10); /* Sensor/CRC error: dashes. */
        }
        co2_indicators();
        __builtin_avr_delay_cycles(F_CPU / 1000UL * 200);
    }
}

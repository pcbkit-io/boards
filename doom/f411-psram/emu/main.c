/* The STM32F411's program: a RISC-V computer for Doom built from serial chips (rvemu.c). Its
 * program (Doom for pcbkit-virt, an ELF file) is in the W25Q128 flash, its 8 MiB RAM is the
 * APS6404L PSRAM, its screen the ST7789 and its GPIO inputs the board's eight buttons.
 *
 *   SPI1 (memories): SCK PA5, MISO PA6, MOSI PA7 at 50 MHz; chip selects PSRAM PA4, flash PB2
 *   SPI2 (display): SCK PB13, MOSI PB15 at 25 MHz; CS PB12, D/C PB14, RESET PB1, backlight PB10
 *   buttons to ground, pulled up: up PA1, down PA2, left PA3, right PB4, fire PB6, use PB7,
 *   strafe PB8, menu PB0 (the guest's GPIO 0…7)
 *
 * Every memory transaction is one cache line, read with fast read (0Bh: a dummy byte) from either
 * chip. Each guest frame (320 × 200, 8-bit) goes to the panel's 320 × 200 window through the
 * guest's colour table as RGB565. Held at power-up, MENU puts the chip in programming mode: it
 * lets go of the memory bus (every pin an input) so a programmer clipped onto the W25Q128 writes
 * the guest in place. */
#include <stdint.h>
#include <pcbkit/stm32f411.h>
#include "rvemu.h"

#define RAM_CS pcbkit_pin(0, 4)
#define ROM_CS pcbkit_pin(1, 2)
#define LCD_CS pcbkit_pin(1, 12)
#define LCD_DC pcbkit_pin(1, 14)
#define LCD_RESET pcbkit_pin(1, 1)
#define LCD_BACKLIGHT pcbkit_pin(1, 10)

static const uint8_t buttons[8] = {pcbkit_pin(0, 1), pcbkit_pin(0, 2), pcbkit_pin(0, 3), pcbkit_pin(1, 4), pcbkit_pin(1, 6), pcbkit_pin(1, 7), pcbkit_pin(1, 8), pcbkit_pin(1, 0)};

/* A transaction's bytes out and in back to back on `spi`: two bytes go in at the start (one
 * shifts, one waits in the transmit buffer); each time a byte has come in, the next goes. `head`
 * bytes first; then `n` bytes from `out` (or zeros) while `in` (if any) takes what comes back. */
static void exchange(uint32_t spi, const uint8_t *head, uint32_t heads, const uint8_t *out, uint8_t *in, uint32_t n) {
    uint32_t total = heads + n;
#define BYTE(i) ((i) < heads ? head[i] : out ? out[(i) - heads] : 0)
    SPI_DR(spi) = BYTE(0);
    if (total > 1) {
        while (!(SPI_SR(spi) & SPI_TXE)) {}
        SPI_DR(spi) = BYTE(1);
    }
    for (uint32_t got = 0; got < total; got++) {
        while (!(SPI_SR(spi) & SPI_RXNE)) {}
        uint8_t b = (uint8_t)SPI_DR(spi);
        if (got + 2 < total) SPI_DR(spi) = BYTE(got + 2);
        if (got >= heads && in) in[got - heads] = b;
    }
    while (SPI_SR(spi) & SPI_BSY) {}
#undef BYTE
}

static void line(unsigned cs, uint8_t op, uint32_t address, const uint8_t *out, uint8_t *in) {
    /* Fast read takes a dummy byte after the address; a write takes none. */
    uint8_t c[5] = {op, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address, 0};
    pcbkit_pin_put(cs, 0);
    exchange(SPI1, c, op == 0x0b ? 5 : 4, out, in, RV_LINE);
    pcbkit_pin_put(cs, 1);
}

void rv_host_read(int flash, uint32_t offset, uint8_t *bytes) { line(flash ? ROM_CS : RAM_CS, 0x0b, offset, 0, bytes); }

void rv_host_write(uint32_t offset, const uint8_t *bytes) { line(RAM_CS, 0x02, offset, bytes, 0); }

uint64_t rv_host_time_us(void) { return pcbkit_time_us(); }

uint32_t rv_host_inputs(void) {
    uint32_t v = ~0u << 8;
    for (unsigned i = 0; i < 8; i++) v |= (uint32_t)pcbkit_pin_get(buttons[i]) << i;
    return v;
}

void rv_host_console(uint8_t byte) { (void)byte; }

static void lcd_command(uint8_t command, const uint8_t *data, uint32_t n) {
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    exchange(SPI2, &command, 1, 0, 0, 0);
    pcbkit_pin_put(LCD_DC, 1);
    if (n) exchange(SPI2, 0, 0, data, 0, n);
    pcbkit_pin_put(LCD_CS, 1);
}

static uint16_t palette565[256];
void rv_host_colour(uint32_t index, uint32_t rgb) {
    uint32_t r = rgb >> 16 & 0xffu, g = rgb >> 8 & 0xffu, b = rgb & 0xffu;
    palette565[index & 255u] = (uint16_t)((r & 0xf8u) << 8 | (g & 0xfcu) << 3 | b >> 3);
}

/* The guest's frame into the panel's 320 × 200 window (rows 20…219 of the landscape 320 × 240). */
void rv_host_present(uint32_t address, uint32_t width, uint32_t height, uint32_t stride) {
    if (width > 320) width = 320;
    if (height > 200) height = 200;
    const uint8_t columns[] = {0, 0, (uint8_t)((width - 1) >> 8), (uint8_t)(width - 1)};
    const uint8_t pages[] = {0, 20, 0, (uint8_t)(19 + height)};
    lcd_command(0x2a, columns, 4);
    lcd_command(0x2b, pages, 4);
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    static const uint8_t write = 0x2c;
    exchange(SPI2, &write, 1, 0, 0, 0);
    pcbkit_pin_put(LCD_DC, 1);
    static uint8_t row[320], out[640];
    for (uint32_t y = 0; y < height; y++) {
        rv_read(address + y * stride, row, width);
        for (uint32_t x = 0; x < width; x++) {
            uint16_t c = palette565[row[x]];
            out[2 * x] = (uint8_t)(c >> 8);
            out[2 * x + 1] = (uint8_t)c;
        }
        exchange(SPI2, 0, 0, out, 0, 2 * width);
    }
    pcbkit_pin_put(LCD_CS, 1);
}

void rv_host_stop(const char *why, uint32_t code, uint32_t pc) {
    (void)why;
    (void)code;
    (void)pc;
    for (;;) __asm__ volatile("wfi");
}

static void lcd_start(void) {
    pcbkit_pin_output(LCD_CS, 1);
    pcbkit_pin_output(LCD_DC, 1);
    pcbkit_pin_output(LCD_RESET, 1);
    pcbkit_pin_output(LCD_BACKLIGHT, 0);
    pcbkit_pin_function(pcbkit_pin(1, 13), 5);
    pcbkit_pin_function(pcbkit_pin(1, 15), 5);
    pcbkit_spi_init(SPI2, 25000000);
    /* Hardware reset: low for 10 µs or more, then 120 ms before commands. */
    pcbkit_pin_put(LCD_RESET, 0);
    pcbkit_sleep_until(pcbkit_time_us() + 1000);
    pcbkit_pin_put(LCD_RESET, 1);
    pcbkit_sleep_until(pcbkit_time_us() + 120000);
    /* The IPS panel (normally black) shows true colours only with inversion on; the module lies
     * turned, its header to the left: row/column exchange and column mirror, RGB. */
    static const uint8_t colmod = 0x55, madctl = 0x60;
    lcd_command(0x3a, &colmod, 1);
    lcd_command(0x36, &madctl, 1);
    lcd_command(0x21, 0, 0);
    lcd_command(0x11, 0, 0);
    pcbkit_sleep_until(pcbkit_time_us() + 120000);
    lcd_command(0x29, 0, 0);
    /* Black over the whole panel once. */
    static const uint8_t all_columns[] = {0, 0, 0x01, 0x3f}, all_pages[] = {0, 0, 0, 0xef};
    lcd_command(0x2a, all_columns, 4);
    lcd_command(0x2b, all_pages, 4);
    static const uint8_t write = 0x2c, black[64];
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    exchange(SPI2, &write, 1, 0, 0, 0);
    pcbkit_pin_put(LCD_DC, 1);
    for (unsigned i = 0; i < 320 * 240 * 2 / sizeof black; i++) exchange(SPI2, 0, 0, black, 0, sizeof black);
    pcbkit_pin_put(LCD_CS, 1);
    pcbkit_pin_put(LCD_BACKLIGHT, 1);
}

int main(void) {
    for (unsigned i = 0; i < 8; i++) pcbkit_pin_input(buttons[i], 1);
    if (!pcbkit_pin_get(buttons[7])) for (;;) __asm__ volatile("wfi");
    pcbkit_pin_output(RAM_CS, 1);
    pcbkit_pin_output(ROM_CS, 1);
    pcbkit_pin_function(pcbkit_pin(0, 5), 5);
    pcbkit_pin_function(pcbkit_pin(0, 6), 5);
    pcbkit_pin_function(pcbkit_pin(0, 7), 5);
    pcbkit_spi_init(SPI1, 50000000);
    /* The PSRAM's power-up time (150 µs), then its reset. */
    pcbkit_sleep_until(pcbkit_time_us() + 200);
    static const uint8_t reset_enable = 0x66, reset = 0x99;
    pcbkit_pin_put(RAM_CS, 0);
    exchange(SPI1, &reset_enable, 1, 0, 0, 0);
    pcbkit_pin_put(RAM_CS, 1);
    pcbkit_pin_put(RAM_CS, 0);
    exchange(SPI1, &reset, 1, 0, 0, 0);
    pcbkit_pin_put(RAM_CS, 1);
    lcd_start();
    rv_run(8u << 20);
    return 0;
}

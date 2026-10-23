/* rp2040-doom's engine on doom-04 (rp2040-tft): a bare RP2040 at 200 MHz with a 16 MB flash, a
 * 1.8" 128 × 160 ST7735S TFT on SPI0 and eight buttons. The board's part of the platform
 * (doom_platform.h): its pins, the display, the buttons and where the heap is.
 *
 *   SPI0 SCK GP18, TX GP19; display CS GP17, D/C (A0) GP20, RESET GP21, backlight GP22
 *   buttons to ground, pulled up: forward GP2, back GP3, left GP4, right GP5, fire GP6, use GP7,
 *   START GP8 (the menu), SELECT GP9 (enter)
 *
 * Each composed 320 × 200 frame is halved (each 2 × 2 block averaged) into the panel's middle
 * 160 × 100, landscape, RGB565 at 15 MHz (the ST7735S's serial write cycle is 66 ns). The game's
 * data, rp2040-doom's compressed DOOM1.WHX, is linked into the flash (the processor's `data`)
 * and read in place. */
#include <string.h>
#include <pcbkit/rp2040.h>
#include "doom_platform.h"

#define LCD_SPI 0
#define LCD_CS 17
#define LCD_DC 20
#define LCD_RESET 21
#define LCD_BACKLIGHT 22
#define TOP 14

static const uint8_t button_pins[PCBKIT_DOOM_KEYS] = {2, 3, 4, 5, 6, 7, 0xff, 0xff, 8, 9, 0xff};

uint64_t pcbkit_doom_time_us(void) { return pcbkit_time_us(); }
void pcbkit_doom_sleep_until(uint64_t us) { pcbkit_sleep_until(us); }

static void command(uint8_t c, const uint8_t *data, unsigned n) {
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    pcbkit_spi_put(LCD_SPI, c);
    pcbkit_spi_wait(LCD_SPI);
    pcbkit_pin_put(LCD_DC, 1);
    for (unsigned i = 0; i < n; i++) pcbkit_spi_put(LCD_SPI, data[i]);
    pcbkit_spi_wait(LCD_SPI);
    pcbkit_pin_put(LCD_CS, 1);
}

static void window(unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    const uint8_t columns[] = {0, (uint8_t)x0, 0, (uint8_t)x1}, rows[] = {0, (uint8_t)y0, 0, (uint8_t)y1};
    command(0x2a, columns, 4);
    command(0x2b, rows, 4);
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    pcbkit_spi_put(LCD_SPI, 0x2c);
    pcbkit_spi_wait(LCD_SPI);
    pcbkit_pin_put(LCD_DC, 1);
}

void pcbkit_doom_board_init(void) {
    for (unsigned i = 0; i < PCBKIT_DOOM_KEYS; i++)
        if (button_pins[i] != 0xff) pcbkit_pin_input(button_pins[i], 1);
    pcbkit_pin_output(LCD_CS, 1);
    pcbkit_pin_output(LCD_DC, 1);
    pcbkit_pin_output(LCD_RESET, 1);
    pcbkit_pin_output(LCD_BACKLIGHT, 0);
    pcbkit_spi_init(LCD_SPI, 19, 18, 15000000);
    /* Hardware reset: low for 10 µs or more, then 120 ms before sleep out. */
    pcbkit_pin_put(LCD_RESET, 0);
    pcbkit_sleep_until(pcbkit_time_us() + 1000);
    pcbkit_pin_put(LCD_RESET, 1);
    pcbkit_sleep_until(pcbkit_time_us() + 120000);
    command(0x11, NULL, 0); /* sleep out */
    pcbkit_sleep_until(pcbkit_time_us() + 120000);
    static const uint8_t colmod = 0x05, madctl = 0x68; /* 16 bits; landscape: MX, MV, BGR */
    command(0x3a, &colmod, 1);
    command(0x36, &madctl, 1);
    command(0x29, NULL, 0); /* display on */
    window(0, 0, 159, 127);
    for (unsigned i = 0; i < 160 * 128 * 2; i++) pcbkit_spi_put(LCD_SPI, 0);
    pcbkit_spi_wait(LCD_SPI);
    pcbkit_pin_put(LCD_CS, 1);
    pcbkit_pin_put(LCD_BACKLIGHT, 1);
}

extern char __heap_start[], __heap_end[];
void pcbkit_doom_heap(uint8_t **base, size_t *size) {
    *base = (uint8_t *)__heap_start;
    *size = (size_t)(__heap_end - __heap_start);
}

uint32_t pcbkit_doom_buttons(void) {
    uint32_t in = SIO_GPIO_IN, held = 0;
    for (unsigned i = 0; i < PCBKIT_DOOM_KEYS; i++)
        if (button_pins[i] != 0xff && !(in >> button_pins[i] & 1)) held |= 1u << i;
    return held;
}

/* Two lines of the frame make one of the panel's: each 2 × 2 block averaged. */
static uint16_t even[320];
void pcbkit_doom_line(int y, const uint16_t *rgb565) {
    if (y == 0) window(0, TOP, 159, TOP + 99);
    if (!(y & 1)) {
        memcpy(even, rgb565, sizeof even);
        return;
    }
    for (int x = 0; x < 160; x++) {
        uint32_t a = even[2 * x], b = even[2 * x + 1], c = rgb565[2 * x], d = rgb565[2 * x + 1];
        uint32_t r = ((a >> 11) + (b >> 11) + (c >> 11) + (d >> 11)) >> 2;
        uint32_t g = ((a >> 5 & 63) + (b >> 5 & 63) + (c >> 5 & 63) + (d >> 5 & 63)) >> 2;
        uint32_t bl = ((a & 31) + (b & 31) + (c & 31) + (d & 31)) >> 2;
        uint32_t p = r << 11 | g << 5 | bl;
        pcbkit_spi_put(LCD_SPI, (uint8_t)(p >> 8));
        pcbkit_spi_put(LCD_SPI, (uint8_t)p);
    }
}
void pcbkit_doom_frame_end(void) {
    pcbkit_spi_wait(LCD_SPI);
    pcbkit_pin_put(LCD_CS, 1);
}

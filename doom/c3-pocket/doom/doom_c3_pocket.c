/* rp2040-doom's engine on doom-03 (c3-pocket): an ESP32-C3 at 160 MHz with a 4 MB flash, a 1.3"
 * 240 × 240 ST7789 IPS panel on GP-SPI2 and eight buttons. The board's part of the platform
 * (doom_platform.h): its pins, the display, the buttons and where the heap is.
 *
 *   SPI2 SCK IO6, MOSI IO7 (mode 3: the module has no chip select); display D/C IO5, RES IO4,
 *   backlight on 3.3 V
 *   buttons to ground, pulled up: forward IO0, back IO1, left IO2, right IO3, fire IO10, use IO20
 *   (RXD0), strafe IO21 (TXD0: held, left and right sidestep), menu IO9 (BOOT); fire also confirms
 *   in the menus (Enter)
 *
 * Each composed 320 × 200 frame is scaled by 3/4 into the panel's middle 240 × 150 (of four
 * columns or lines the middle two averaged), RGB565 at 40 MHz: 72 KB per frame, 14 ms. The game's
 * data, rp2040-doom's compressed DOOM1.WHX, is linked into the flash (the processor's `data`)
 * and read in place through the cache. */
#include <string.h>
#include <pcbkit/esp32c3.h>
#include "doom_platform.h"

#define LCD_DC 5
#define LCD_RESET 4
#define TOP 45

enum { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3, FIRE = 10, USE = 20, STRAFE = 21, MENU = 9 };
static const uint8_t buttons[] = {UP, DOWN, LEFT, RIGHT, FIRE, USE, STRAFE, MENU};

uint64_t pcbkit_doom_time_us(void) { return pcbkit_time_us(); }
void pcbkit_doom_sleep_until(uint64_t us) { pcbkit_sleep_until(us); }

static void command(uint8_t c, const uint8_t *data, unsigned n) {
    pcbkit_pin_put(LCD_DC, 0);
    pcbkit_spi_send(-1, &c, 1);
    pcbkit_pin_put(LCD_DC, 1);
    if (n) pcbkit_spi_write(data, n);
}

static void window(unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    const uint8_t columns[] = {0, (uint8_t)x0, 0, (uint8_t)x1}, rows[] = {0, (uint8_t)y0, 0, (uint8_t)y1};
    command(0x2a, columns, 4);
    command(0x2b, rows, 4);
    command(0x2c, NULL, 0);
}

void pcbkit_doom_board_init(void) {
    for (unsigned i = 0; i < sizeof buttons; i++) pcbkit_pin_input(buttons[i], 1);
    pcbkit_pin_output(LCD_DC, 1);
    pcbkit_pin_output(LCD_RESET, 1);
    pcbkit_spi_init(2, 3);
    /* Hardware reset: low for 10 µs or more, then 120 ms before sleep out. */
    pcbkit_pin_put(LCD_RESET, 0);
    pcbkit_sleep_until(pcbkit_time_us() + 1000);
    pcbkit_pin_put(LCD_RESET, 1);
    pcbkit_sleep_until(pcbkit_time_us() + 120000);
    command(0x11, NULL, 0); /* sleep out */
    pcbkit_sleep_until(pcbkit_time_us() + 120000);
    static const uint8_t colmod = 0x55, madctl = 0x00; /* 16 bits; portrait, RGB order */
    command(0x3a, &colmod, 1);
    command(0x36, &madctl, 1);
    command(0x21, NULL, 0); /* inversion on: the IPS panel shows true colours */
    command(0x29, NULL, 0); /* display on */
    window(0, 0, 239, 239);
    static const uint8_t black[64];
    for (unsigned i = 0; i < 240 * 240 * 2; i += sizeof black) pcbkit_spi_write(black, sizeof black);
}

extern char __heap_start[], __heap_end[];
void pcbkit_doom_heap(uint8_t **base, size_t *size) {
    *base = (uint8_t *)__heap_start;
    *size = (size_t)(__heap_end - __heap_start);
}

uint32_t pcbkit_doom_buttons(void) {
    uint32_t low = ~GPIO_IN, held = 0;
    int strafe = low >> STRAFE & 1;
    if (low >> UP & 1) held |= 1u << PCBKIT_DOOM_UP;
    if (low >> DOWN & 1) held |= 1u << PCBKIT_DOOM_DOWN;
    if (low >> LEFT & 1) held |= 1u << (strafe ? PCBKIT_DOOM_STRAFE_LEFT : PCBKIT_DOOM_LEFT);
    if (low >> RIGHT & 1) held |= 1u << (strafe ? PCBKIT_DOOM_STRAFE_RIGHT : PCBKIT_DOOM_RIGHT);
    if (low >> FIRE & 1) held |= 1u << PCBKIT_DOOM_FIRE | 1u << PCBKIT_DOOM_ENTER;
    if (low >> USE & 1) held |= 1u << PCBKIT_DOOM_USE;
    if (low >> MENU & 1) held |= 1u << PCBKIT_DOOM_MENU;
    return held;
}

/* 4 → 3 in both directions: of four pixels (or lines) the first, the middle two averaged, the
 * last. Two RGB565 pixels averaged per channel without carries between them. */
static inline uint16_t mean(uint16_t a, uint16_t b) { return (uint16_t)(((a ^ b) & 0xf7deu) >> 1) + (a & b); }

static uint16_t held_line[320];
static uint8_t out[240 * 2];

static void send(const uint16_t *line) {
    for (int x = 0, o = 0; x < 320; x += 4) {
        uint16_t p[3] = {line[x], mean(line[x + 1], line[x + 2]), line[x + 3]};
        for (int k = 0; k < 3; k++, o += 2) {
            out[o] = (uint8_t)(p[k] >> 8);
            out[o + 1] = (uint8_t)p[k];
        }
    }
    pcbkit_spi_write(out, sizeof out);
}

void pcbkit_doom_line(int y, const uint16_t *rgb565) {
    if (y == 0) window(0, TOP, 239, TOP + 149);
    switch (y & 3) {
    case 0:
    case 3: send(rgb565); break;
    case 1: memcpy(held_line, rgb565, sizeof held_line); break;
    case 2: {
        uint16_t m[320];
        for (int x = 0; x < 320; x++) m[x] = mean(held_line[x], rgb565[x]);
        send(m);
        break;
    }
    }
}
void pcbkit_doom_frame_end(void) {}

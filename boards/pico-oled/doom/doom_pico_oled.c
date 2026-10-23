/* rp2040-doom's engine on doom-02 (pico-oled): a Raspberry Pi Pico (RP2040 at 200 MHz, 2 MB flash)
 * behind a 2.42" 128 × 64 monochrome OLED (SSD1309, 4-wire SPI) and eight buttons. The board's part
 * of the platform (doom_platform.h): its pins, the display, the buttons and where the heap is.
 *
 *   SPI0 SCK GP18, TX GP19; display CS GP17, D/C GP20, RES GP21
 *   buttons to ground, pulled up: forward GP2, back GP3, left GP4, right GP5, fire GP6, use GP7,
 *   strafe GP8 (held, left and right sidestep), menu GP9; fire also confirms in the menus (Enter)
 *
 * Each composed 320 × 200 frame is reduced to 128 × 64 (every pixel the mean brightness of the
 * source pixels it covers), its levels stretched so that the darkest and the brightest 2 % of the
 * previous frame's pixels are black and white (Doom's scenes are dim and of few tones), dithered to
 * one bit by Atkinson's diffusion (only 3/4 of the error spread: flat areas stay clean, edges
 * keep their contrast) and sent as the controller's eight pages, 1 KB per frame at 10 MHz
 * (0.8 ms). The
 * game's data, rp2040-doom's compressed DOOM1.WHX, is linked into the flash (the processor's
 * `data`) and read in place. */
#include <string.h>
#include <pcbkit/rp2040.h>
#include "doom_platform.h"

#define LCD_SPI 0
#define LCD_CS 17
#define LCD_DC 20
#define LCD_RESET 21
#define W 128
#define H 64

enum { UP = 2, DOWN, LEFT, RIGHT, FIRE, USE, STRAFE, MENU };

uint64_t pcbkit_doom_time_us(void) { return pcbkit_time_us(); }
void pcbkit_doom_sleep_until(uint64_t us) { pcbkit_sleep_until(us); }

static void commands(const uint8_t *c, unsigned n) {
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    for (unsigned i = 0; i < n; i++) pcbkit_spi_put(LCD_SPI, c[i]);
    pcbkit_spi_wait(LCD_SPI);
    pcbkit_pin_put(LCD_CS, 1);
}

/* The levels (brightness → stretched brightness), each source column's output column and 65536 / n
 * (the Cortex-M0+ has no divide instruction). */
static uint8_t levels[256], column[320];
static uint16_t reciprocal[16];

void pcbkit_doom_board_init(void) {
    for (unsigned pin = UP; pin <= MENU; pin++) pcbkit_pin_input(pin, 1);
    pcbkit_pin_output(LCD_CS, 1);
    pcbkit_pin_output(LCD_DC, 1);
    pcbkit_pin_output(LCD_RESET, 1);
    pcbkit_spi_init(LCD_SPI, 19, 18, 10000000);
    for (unsigned v = 0; v < 256; v++) levels[v] = (uint8_t)v;
    for (unsigned x = 0; x < 320; x++) column[x] = (uint8_t)(x * W / 320);
    for (unsigned n = 1; n < 16; n++) reciprocal[n] = (uint16_t)(65535 / n);
    /* Reset: low for 3 µs or more; then the SSD1309's set-up (its datasheet's application
     * example): off, clock, 64 rows, no offset, start line 0, column 127 at SEG0 and COM scan
     * reversed (the glass the right way up with the header on the left), alternative COM pins,
     * contrast, pre-charge, VCOMH, RAM shown, not inverted, horizontal addressing over the whole
     * glass, on. */
    pcbkit_pin_put(LCD_RESET, 0);
    pcbkit_sleep_until(pcbkit_time_us() + 1000);
    pcbkit_pin_put(LCD_RESET, 1);
    pcbkit_sleep_until(pcbkit_time_us() + 1000);
    static const uint8_t init[] = {0xae, 0xd5, 0xa0, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0xa1, 0xc8, 0xda, 0x12, 0x81, 0xdf,
                                   0xd9, 0x82, 0xdb, 0x34, 0xa4, 0xa6, 0x20, 0x00, 0x21, 0, W - 1, 0x22, 0, H / 8 - 1, 0xaf};
    commands(init, sizeof init);
}

extern char __heap_start[], __heap_end[];
void pcbkit_doom_heap(uint8_t **base, size_t *size) {
    *base = (uint8_t *)__heap_start;
    *size = (size_t)(__heap_end - __heap_start);
}

uint32_t pcbkit_doom_buttons(void) {
    uint32_t low = ~SIO_GPIO_IN, held = 0;
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

/* The output row being gathered (sums and counts of brightness per column), the dithering's error
 * for this row and the two below it (rotating), this frame's histogram, and the frame in the
 * controller's page layout. */
static uint32_t sum[W];
static uint16_t count[W];
static int16_t error[3][W + 4];
static uint8_t current;
static uint16_t histogram[256];
static uint8_t pages[W * H / 8];
static int row;

static void emit(int oy) {
    int16_t *e0 = error[current], *e1 = error[current == 2 ? 0 : current + 1], *e2 = error[current == 0 ? 2 : current - 1];
    for (int x = 0; x < W; x++) {
        int g = (int)(sum[x] * reciprocal[count[x]] >> 16);
        histogram[g]++;
        int v = levels[g] + e0[x + 2];
        int on = v >= 128, e = (v - (on ? 255 : 0)) / 8;
        if (on) pages[(oy >> 3) * W + x] |= 1u << (oy & 7);
        e0[x + 3] += e;
        e0[x + 4] += e;
        e1[x + 1] += e;
        e1[x + 2] += e;
        e1[x + 3] += e;
        e2[x + 2] += e;
    }
    memset(e0, 0, sizeof error[0]);
    current = current == 2 ? 0 : current + 1;
    memset(sum, 0, sizeof sum);
    memset(count, 0, sizeof count);
}

/* The next frame's levels from this one's histogram: 2 % of the pixels at each end clipped, at
 * least 16 steps between black and white. */
static void stretch(void) {
    unsigned lo = 0, hi = 255, seen = 0;
    while (lo < 255 && (seen += histogram[lo]) <= W * H / 50) lo++;
    for (seen = 0; hi > 0 && (seen += histogram[hi]) <= W * H / 50; hi--) {}
    if (hi < lo + 16) hi = lo + 16 > 255 ? 255 : lo + 16;
    if (lo > hi - 16) lo = hi - 16;
    uint32_t k = (255u << 16) / (hi - lo);
    for (unsigned v = 0; v < 256; v++) levels[v] = v <= lo ? 0 : v >= hi ? 255 : (uint8_t)((v - lo) * k >> 16);
    memset(histogram, 0, sizeof histogram);
}

void pcbkit_doom_line(int y, const uint16_t *rgb565) {
    int oy = y * H / 200;
    if (y == 0) {
        memset(pages, 0, sizeof pages);
        memset(error, 0, sizeof error);
        current = 0;
        row = 0;
    }
    if (oy != row) {
        emit(row);
        row = oy;
    }
    for (int x = 0; x < 320; x++) {
        uint32_t p = rgb565[x];
        /* 8-bit luma from 5/6/5 bits: 0.30 R + 0.59 G + 0.11 B. */
        uint32_t l = ((p >> 11) * 630 + (p >> 5 & 63) * 608 + (p & 31) * 240) >> 8;
        sum[column[x]] += l;
        count[column[x]]++;
    }
}

void pcbkit_doom_frame_end(void) {
    emit(row);
    stretch();
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 1);
    for (unsigned i = 0; i < sizeof pages; i++) pcbkit_spi_put(LCD_SPI, pages[i]);
    pcbkit_spi_wait(LCD_SPI);
    pcbkit_pin_put(LCD_CS, 1);
}

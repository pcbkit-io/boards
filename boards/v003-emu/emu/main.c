/* The CH32V003's program: a RISC-V computer for Doom built from serial chips (rvemu.c). Its
 * program (Doom for pcbkit-virt, an ELF file) is in the W25Q128 flash, its 8 MiB RAM is the
 * APS6404L PSRAM, its screen the Nokia 5110 and its GPIO inputs the board's eight buttons.
 *
 *   SPI1: SCK PC5, MOSI PC6, MISO PC7 at 24 MHz; chip selects PSRAM PC0, flash PC4, LCD PC3;
 *   LCD D/C PC2 (its reset is an RC network); buttons to ground, pulled up: up PD0, down PD2,
 *   left PD3, right PD4, fire PD5, use PD6, strafe PA1, menu PA2 (the guest's GPIO 0…7).
 *
 * Every memory transaction is one cache line: the command, the 24-bit address and 16 bytes,
 * 20 bytes at 24 MHz, 6.7 µs with chip select low: inside the PSRAM's 8 µs limit (tCEM). The
 * guest's frames (320 × 200, 8-bit) are scaled to the 84 × 48 glass with ordered dithering.
 *
 * Held at power-up, MENU puts the chip in programming mode: it lets go of the bus (every SPI pin
 * an input) and waits, so a programmer clipped onto the W25Q128 (SOIC-8) writes the guest in
 * place while the board is switched on. */
#include <stdint.h>
#include <pcbkit/ch32v003.h>
#include "rvemu.h"

const struct pcbkit_board pcbkit_board = {.uart_baud = 0};

#define RAM_CS pcbkit_pin(1, 0)
#define ROM_CS pcbkit_pin(1, 4)
#define LCD_CS pcbkit_pin(1, 3)
#define LCD_DC pcbkit_pin(1, 2)

static const uint8_t buttons[8] = {pcbkit_pin(2, 0), pcbkit_pin(2, 2), pcbkit_pin(2, 3), pcbkit_pin(2, 4), pcbkit_pin(2, 5), pcbkit_pin(2, 6), pcbkit_pin(0, 1), pcbkit_pin(0, 2)};

/* A transaction's bytes out and in back to back, the bus never idle (each byte 333 ns at 24 MHz,
 * 16 cycles of the core): two bytes go in at the start (one shifts, one waits in the transmit
 * buffer); each time a byte has come in, the waiting one is shifting and the buffer is free for
 * the next. `head` bytes (a command and its address) go first; then `n` bytes from `out` (or
 * zeros) while `in` (if any) takes what comes back for them. */
static void exchange(const uint8_t *head, uint32_t heads, const uint8_t *out, uint8_t *in, uint32_t n) {
    volatile uint16_t *status = &SPI_STATR(SPI1), *data = &SPI_DATAR(SPI1);
    uint32_t total = heads + n;
#define BYTE(i) ((i) < heads ? head[i] : out ? out[(i) - heads] : 0)
    *data = BYTE(0);
    if (total > 1) *data = BYTE(1);
    for (uint32_t got = 0; got < total; got++) {
        while (!(*status & SPI_RXNE)) {}
        uint8_t b = (uint8_t)*data;
        if (got + 2 < total) *data = BYTE(got + 2);
        if (got >= heads && in) in[got - heads] = b;
    }
#undef BYTE
}

static void transaction(unsigned cs, uint8_t op, uint32_t address, const uint8_t *out, uint8_t *in) {
    uint8_t c[4] = {op, (uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)address};
    pcbkit_pin_put(cs, 0);
    exchange(c, 4, out, in, RV_LINE);
    pcbkit_pin_put(cs, 1);
}

void rv_host_read(int flash, uint32_t offset, uint8_t *line) { transaction(flash ? ROM_CS : RAM_CS, 0x03, offset, 0, line); }

void rv_host_write(uint32_t offset, const uint8_t *line) { transaction(RAM_CS, 0x02, offset, line, 0); }

uint64_t rv_host_time_us(void) { return pcbkit_time_us(); }

uint32_t rv_host_inputs(void) {
    uint32_t v = ~0u << 8;
    for (unsigned i = 0; i < 8; i++) v |= (uint32_t)pcbkit_pin_get(buttons[i]) << i;
    return v;
}


static void lcd(int data, const uint8_t *bytes, uint32_t n) {
    pcbkit_pin_put(LCD_DC, data);
    pcbkit_pin_put(LCD_CS, 0);
    exchange(0, 0, bytes, 0, n);
    pcbkit_pin_put(LCD_CS, 1);
}

/* Whether the guest has shown a frame (its console stays off the glass from then). */
static int presented;

/* 4 × 4 ordered dither thresholds, 0…15. */
static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

void rv_host_present(uint32_t address, uint32_t width, uint32_t height, uint32_t stride, const uint8_t *luma) {
    static const uint8_t home[2] = {0x40, 0x80};
    presented = 1;
    lcd(0, home, 2);
    uint8_t column[84];
    for (uint32_t bank = 0; bank < 6; bank++) {
        for (uint32_t x = 0; x < 84; x++) {
            uint8_t bits = 0;
            uint32_t sx = (x * width + width / 2) / 84;
            for (uint32_t r = 0; r < 8; r++) {
                uint32_t y = bank * 8 + r, sy = (y * height + height / 2) / 48;
                uint8_t p;
                rv_read(address + sy * stride + sx, &p, 1);
                /* A dark pixel where the scene is darker than the threshold. */
                if (luma[p] <= bayer[(y & 3) * 4 + (x & 3)]) bits |= (uint8_t)(1u << r);
            }
            column[x] = bits;
        }
        lcd(1, column, 84);
    }
}

/* A 3 × 5 font for messages on the glass: 21 characters on each of 8 lines. */
static const char glyphs[] = " '(),-./0123456789:=?ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const uint8_t font[][3] = {{0,0,0}, {0,3,0}, {14,17,0}, {0,17,14}, {16,8,0}, {4,4,4}, {0,16,0}, {24,4,3}, {31,17,31}, {18,31,16}, {29,21,23}, {17,21,31}, {7,4,31}, {23,21,29}, {31,21,29}, {1,29,3}, {31,21,31}, {23,21,31}, {0,10,0}, {10,10,10}, {1,21,3}, {31,5,31}, {31,21,10}, {31,17,17}, {31,17,14}, {31,21,17}, {31,5,1}, {31,17,29}, {31,4,31}, {17,31,17}, {24,16,31}, {31,4,27}, {31,16,16}, {31,6,31}, {31,1,30}, {31,17,31}, {31,5,7}, {15,9,31}, {31,5,26}, {23,21,29}, {1,31,1}, {31,16,31}, {15,16,15}, {31,12,31}, {27,4,27}, {3,28,3}, {25,21,19}};

/* Text on the glass from its top left, 21 characters a line (a newline ends one); lowercase in
 * capitals. The rest of the screen is cleared. */
static void say(const char *text) {
    static const uint8_t home[2] = {0x40, 0x80};
    lcd(0, home, 2);
    uint8_t row[84];
    for (unsigned bank = 0; bank < 6; bank++) {
        for (unsigned i = 0; i < 84; i++) row[i] = 0;
        for (unsigned x = 0; x < 84 && *text; x += 4) {
            if (*text == '\n') {
                text++;
                break;
            }
            char c = *text++;
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            unsigned g = 0;
            while (glyphs[g] && glyphs[g] != c) g++;
            if (!glyphs[g]) g = 20; /* ? */
            for (unsigned k = 0; k < 3; k++) row[x + k] = (uint8_t)(font[g][k] << 1);
        }
        lcd(1, row, 84);
    }
}

/* The guest's console, a line at a time on the glass until its first frame: while Doom loads
 * (minutes at an emulator's pace) its start-up messages show how far it has come. */
void rv_host_console(uint8_t byte) {
    static char line[22];
    static unsigned n;
    if (presented) return;
    if (byte != '\n' && n < sizeof line - 1) {
        if (byte >= ' ') line[n++] = (char)byte;
        return;
    }
    line[n] = 0;
    if (n) say(line);
    n = 0;
}

static void hex(char *to, uint32_t v) {
    for (int i = 7; i >= 0; i--, v >>= 4) to[i] = "0123456789ABCDEF"[v & 15];
}

void rv_host_stop(const char *why, uint32_t code, uint32_t pc) {
    /* The guest stopped: the reason on the glass (a powered-off guest leaves its last frame). */
    if (why || code) {
        static char text[160];
        unsigned n = 0;
        const char *head = why ? why : "the program failed with code";
        while (*head && n < 120) text[n++] = *head++;
        text[n++] = '\n';
        if (!why) {
            hex(text + n, code);
            n += 8;
            text[n++] = '\n';
        }
        text[n++] = 'P';
        text[n++] = 'C';
        text[n++] = ' ';
        hex(text + n, pc);
        n += 8;
        text[n] = 0;
        say(text);
    }
    for (;;) __asm__ volatile("wfi");
}

int main(void) {
    for (unsigned i = 0; i < 8; i++) pcbkit_pin_input(buttons[i], 1);
    if (!pcbkit_pin_get(buttons[7])) {
        static const uint8_t bus[] = {pcbkit_pin(1, 0), pcbkit_pin(1, 2), pcbkit_pin(1, 3), pcbkit_pin(1, 4), pcbkit_pin(1, 5), pcbkit_pin(1, 6), pcbkit_pin(1, 7)};
        for (unsigned i = 0; i < sizeof bus; i++) pcbkit_pin_mode(bus[i], GPIO_IN_FLOATING);
        for (;;) __asm__ volatile("wfi");
    }
    pcbkit_pin_output(RAM_CS, 1);
    pcbkit_pin_output(ROM_CS, 1);
    pcbkit_pin_output(LCD_CS, 1);
    pcbkit_pin_output(LCD_DC, 0);
    pcbkit_spi_init(24000000);
    /* The PSRAM's power-up time (150 µs), then its reset; the LCD's RC reset has ended by now. */
    pcbkit_sleep_until(pcbkit_time_us() + 200);
    static const uint8_t reset_enable = 0x66, reset = 0x99;
    pcbkit_pin_put(RAM_CS, 0);
    exchange(&reset_enable, 1, 0, 0, 0);
    pcbkit_pin_put(RAM_CS, 1);
    pcbkit_pin_put(RAM_CS, 0);
    exchange(&reset, 1, 0, 0, 0);
    pcbkit_pin_put(RAM_CS, 1);
    /* LCD: extended set, Vop 0x38, temperature coefficient 0, bias 1:48; basic set, normal. */
    static const uint8_t init[] = {0x21, 0xb8, 0x04, 0x14, 0x20, 0x0c};
    lcd(0, init, sizeof init);
    rv_run(8u << 20);
    return 0;
}

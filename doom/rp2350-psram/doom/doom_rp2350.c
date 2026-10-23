/* Doom's platform layer for doom-07: a bare RP2350A (RISC-V cores) with a 16 MiB flash and an
 * 8 MiB PSRAM on QMI chip select 1 (GPIO8), a 2.0" ST7789 240 × 320 IPS on SPI0 and ten buttons.
 * An ordinary program on the chip's devices:
 *
 *   SPI0 SCK GPIO18, TX GPIO19; display CS GPIO17, D/C GPIO16, RESET GPIO20, backlight GPIO21
 *   buttons to ground, pulled up: forward GPIO0, back 1, left 2, right 3, fire 4, use 5,
 *   strafe left 9, strafe right 15, menu 25, enter 26
 *   no console: GPIO0 and GPIO1 are buttons
 *
 * Each frame converts the 8-bit screen through Doom's palette to RGB565 and writes it to the
 * panel's 320 × 200 window (rows 20…219 of the landscape 320 × 240). The game's data, the
 * shareware DOOM1.WAD, is linked into flash (the processor's `data` in the design) and read in
 * place; the zone heap and the screens live in PSRAM. Waiting for the next tic sleeps
 * until a timer alarm. Audio and saved games are absent. The engine is sysprog21/doom_riscv
 * (GPL-2.0), unchanged at its pinned revision. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcbkit/rp2350.h>
#include <pcbkit/runtime.h>
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

const struct pcbkit_board pcbkit_board = {
    .xosc_hz = 12000000, .pll_fbdiv = 125, .pll_postdiv1 = 5, .pll_postdiv2 = 2,
    .psram_cs = 8, .uart_tx = -1, .uart_rx = -1,
};

#define LCD_CS 17
#define LCD_DC 16
#define LCD_RESET 20
#define LCD_BACKLIGHT 21
#define BUTTONS 10u
/* forward, back, left, right, fire, use, strafe left, strafe right, menu, enter */
static const uint8_t button_pins[BUTTONS] = {0, 1, 2, 3, 4, 5, 9, 15, 25, 26};
#define TIC_US (1000000u / TICRATE)

static uint64_t origin;
static uint32_t held;
static uint16_t palette565[256];

static void lcd_command(uint8_t command, const uint8_t *data, size_t n) {
    pcbkit_gpio_put(LCD_CS, 0);
    pcbkit_gpio_put(LCD_DC, 0);
    pcbkit_spi_write(SPI0, &command, 1);
    pcbkit_spi_wait(SPI0); /* D/C must not change under the command byte */
    pcbkit_gpio_put(LCD_DC, 1);
    pcbkit_spi_write(SPI0, data, n);
    pcbkit_spi_wait(SPI0);
    pcbkit_gpio_put(LCD_CS, 1);
}

static void sleep_ms(unsigned ms) { pcbkit_sleep_until(pcbkit_time_us() + ms * 1000u); }

static void lcd_start(void) {
    pcbkit_gpio_output(LCD_CS, 1);
    pcbkit_gpio_output(LCD_DC, 1);
    pcbkit_gpio_output(LCD_RESET, 1);
    pcbkit_gpio_output(LCD_BACKLIGHT, 0);
    pcbkit_gpio_function(18, FUNC_SPI);
    pcbkit_gpio_function(19, FUNC_SPI);
    printf("LCD SPI %lu Hz\n", (unsigned long)pcbkit_spi_init(SPI0, 25000000));
    /* Hardware reset: low for 10 µs or more, then 120 ms before commands. */
    pcbkit_gpio_put(LCD_RESET, 0);
    sleep_ms(1);
    pcbkit_gpio_put(LCD_RESET, 1);
    sleep_ms(120);
    /* The module's panel is IPS (normally black): colours are true only with inversion on. On
     * the board the module lies turned, its header to the left: the landscape picture is the
     * memory's rows across and its columns upward (row/column exchange, column mirror), RGB. */
    static const uint8_t init[] = {
        0x3a, 1, 0x55,
        0x36, 1, 0x60,
        0,
    };
    for (const uint8_t *p = init; *p; p += 2 + p[1]) lcd_command(p[0], p + 2, p[1]);
    lcd_command(0x21, NULL, 0); /* inversion on */
    lcd_command(0x11, NULL, 0); /* sleep out */
    sleep_ms(120);
    lcd_command(0x29, NULL, 0); /* display on */
    /* Clear the whole panel once, then the game's window only. */
    static const uint8_t all_columns[] = {0, 0, 0x01, 0x3f}, all_pages[] = {0, 0, 0, 0xef};
    lcd_command(0x2a, all_columns, 4);
    lcd_command(0x2b, all_pages, 4);
    static const uint8_t black[64];
    pcbkit_gpio_put(LCD_CS, 0);
    pcbkit_gpio_put(LCD_DC, 0);
    pcbkit_spi_write(SPI0, (const uint8_t[]){0x2c}, 1);
    pcbkit_spi_wait(SPI0);
    pcbkit_gpio_put(LCD_DC, 1);
    for (unsigned i = 0; i < 320 * 240 * 2 / sizeof black; i++) pcbkit_spi_write(SPI0, black, sizeof black);
    pcbkit_spi_wait(SPI0);
    pcbkit_gpio_put(LCD_CS, 1);
    pcbkit_gpio_put(LCD_BACKLIGHT, 1);
}

void I_Init(void) {
    origin = pcbkit_time_us();
}

byte *I_ZoneBase(int *size) {
    *size = 5 * 1024 * 1024;
    return malloc(*size);
}

/* The current tic. A caller that polls without the tic moving is waiting for the next one:
 * after 16 unchanged polls, sleep until it starts. */
int I_GetTime(void) {
    static uint64_t last = UINT64_MAX;
    static unsigned unchanged;
    uint64_t tic = (pcbkit_time_us() - origin) / TIC_US;
    if (tic != last) {
        unchanged = 0;
    } else if (++unchanged >= 16) {
        pcbkit_sleep_until(origin + (tic + 1) * TIC_US);
        tic = (pcbkit_time_us() - origin) / TIC_US;
        unchanged = 0;
    }
    last = tic;
    return (int)tic;
}

void I_StartFrame(void) {}

void I_StartTic(void) {
    static const int keys[BUTTONS] = {KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW, KEY_RCTRL, ' ', ',', '.', KEY_ESCAPE, KEY_ENTER};
    uint32_t in = REG(SIO_GPIO_IN), pressed = 0;
    for (unsigned i = 0; i < BUTTONS; i++) pressed |= (~in >> button_pins[i] & 1u) << i;
    uint32_t changed = pressed ^ held;
    held = pressed;
    for (unsigned i = 0; i < BUTTONS; i++) {
        if (changed & (1u << i)) {
            event_t event = {.type = pressed & (1u << i) ? ev_keydown : ev_keyup, .data1 = keys[i]};
            D_PostEvent(&event);
        }
    }
}

ticcmd_t *I_BaseTiccmd(void) {
    static ticcmd_t empty;
    return &empty;
}

void I_Quit(void) { exit(0); }

/* The four screens (256,000 bytes) in PSRAM: with the full game's tables and visplanes, SRAM
 * keeps the engine's statics and the stack. */
static PCBKIT_PSRAM byte low[SCREENWIDTH * SCREENHEIGHT * 4];
byte *I_AllocLow(int length) {
    if ((size_t)length <= sizeof low) {
        memset(low, 0, (size_t)length);
        return low;
    }
    return calloc(1, (size_t)length);
}
void I_Tactile(int on, int off, int total) { (void)on; (void)off; (void)total; }

void I_Error(char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

void I_InitGraphics(void) {
    usegamma = 0;
    lcd_start();
}
void I_ShutdownGraphics(void) {}

void I_SetPalette(byte *palette) {
    for (int i = 0; i < 256; i++, palette += 3) palette565[i] = (uint16_t)((palette[0] & 0xf8) << 8 | (palette[1] & 0xfc) << 3 | palette[2] >> 3);
}

void I_SetRelativeMode(boolean enabled) { (void)enabled; }
void I_MarkDirtyLines(int first, int last) { (void)first; (void)last; }
void I_UpdateNoBlit(void) {}

void I_FinishUpdate(void) {
    static const uint8_t columns[] = {0, 0, 0x01, 0x3f}, pages[] = {0, 20, 0, 219};
    lcd_command(0x2a, columns, 4);
    lcd_command(0x2b, pages, 4);
    pcbkit_gpio_put(LCD_CS, 0);
    pcbkit_gpio_put(LCD_DC, 0);
    pcbkit_spi_write(SPI0, (const uint8_t[]){0x2c}, 1);
    pcbkit_spi_wait(SPI0);
    pcbkit_gpio_put(LCD_DC, 1);
    static uint8_t line[SCREENWIDTH * 2];
    const byte *src = screens[0];
    for (int y = 0; y < SCREENHEIGHT; y++) {
        for (int x = 0; x < SCREENWIDTH; x++) {
            uint16_t c = palette565[*src++];
            line[2 * x] = (uint8_t)(c >> 8);
            line[2 * x + 1] = (uint8_t)c;
        }
        pcbkit_spi_write(SPI0, line, sizeof line);
    }
    pcbkit_spi_wait(SPI0);
    pcbkit_gpio_put(LCD_CS, 1);
}
void I_WaitVBL(int count) { (void)count; }
void I_ReadScreen(byte *dest) { memcpy(dest, screens[0], SCREENWIDTH * SCREENHEIGHT); }
void I_BeginRead(void) {}
void I_EndRead(void) {}

/* No audio. Doom's noise propagation (P_NoiseAlert) is gameplay and stays in the engine. */
int snd_SfxVolume = 0, snd_MusicVolume = 0, numChannels = 0;
void S_Init(int sfx, int music) { (void)sfx; (void)music; }
void S_Start(void) {}
void S_StartSound(void *origin_, int id) { (void)origin_; (void)id; }
void S_StartSoundAtVolume(void *origin_, int id, int volume) { (void)origin_; (void)id; (void)volume; }
void S_StopSound(void *origin_) { (void)origin_; }
void S_StartMusic(int id) { (void)id; }
void S_ChangeMusic(int id, int looping) { (void)id; (void)looping; }
void S_StopMusic(void) {}
void S_PauseSound(void) {}
void S_ResumeSound(void) {}
void S_UpdateSounds(void *listener) { (void)listener; }
void S_SetMusicVolume(int volume) { snd_MusicVolume = volume; }
void S_SetSfxVolume(int volume) { snd_SfxVolume = volume; }
void I_ShutdownSound(void) {}

int main(void) {
    for (unsigned i = 0; i < BUTTONS; i++) pcbkit_gpio_input(button_pins[i], 1);
    printf("Doom on doom-07 (RP2350A): %lu Hz, PSRAM %u KiB\n", (unsigned long)pcbkit_sys_hz, (unsigned)(pcbkit_psram_bytes >> 10));
    origin = pcbkit_time_us();
    D_DoomMain();
    return 0;
}

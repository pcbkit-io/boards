/* Doom's platform layer for doom-08: an ESP32-S3-WROOM-1-N16R8 (Xtensa LX7, 16 MiB flash, 8 MiB
 * octal PSRAM) under a 2.8" ILI9341 240 × 320 TFT on GP-SPI2 and ten buttons. An ordinary
 * program on the chip's devices:
 *
 *   GP-SPI2 on its IO_MUX pads: SCK IO12, MOSI IO11; display CS IO10, D/C IO9, RESET IO14,
 *   backlight IO21; the touch controller's CS IO47 held high
 *   buttons to ground, pulled up: forward IO15, back IO16, left IO17, right IO7, fire IO2,
 *   use IO41, strafe left IO42, strafe right IO1, menu IO0 (BOOT), enter IO44 (RXD0)
 *
 * The program and the game's data, the shareware DOOM1.WAD (the processor's `data` in the
 * design), run and are read in place from flash through the caches; the zone heap, the screens
 * and the visplanes are in the PSRAM. Each frame goes through Doom's palette to RGB565 and into
 * the panel's 320 × 200 window (rows 20…219 of the landscape 320 × 240), 64 bytes a GP-SPI2
 * transaction at 40 MHz. Waiting for the next tic sleeps (WAITI until CCOMPARE0). Audio and saved
 * games are absent. The engine is sysprog21/doom_riscv (GPL-2.0) at its pinned revision, its
 * tables const (tables.c/h, r_main.c: in flash) and two arrays in the PSRAM (r_plane.c, w_wad.c). */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcbkit/esp32s3.h>
#include <pcbkit/runtime.h>
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#define LCD_CS 10
#define LCD_DC 9
#define LCD_RESET 14
#define LCD_BACKLIGHT 21
#define TOUCH_CS 47
#define BUTTONS 10u
#define TIC_US (1000000u / TICRATE)

/* forward, back, left, right, fire, use, strafe left, strafe right, menu, enter */
static const uint8_t button_pins[BUTTONS] = {15, 16, 17, 7, 2, 41, 42, 1, 0, 44};

static uint64_t origin;
static uint32_t held;
static uint16_t palette565[256];

static void lcd_command(uint8_t command, const uint8_t *data, unsigned n) {
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    pcbkit_spi_send(-1, &command, 1);
    pcbkit_pin_put(LCD_DC, 1);
    if (n) pcbkit_spi_send(-1, data, n);
    pcbkit_pin_put(LCD_CS, 1);
}

/* RAMWR over a window; the caller sends the pixels and raises CS. */
static void lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    const uint8_t columns[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    const uint8_t pages[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_command(0x2a, columns, 4);
    lcd_command(0x2b, pages, 4);
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    pcbkit_spi_send(-1, (const uint8_t[]){0x2c}, 1);
    pcbkit_pin_put(LCD_DC, 1);
}

static void sleep_ms(unsigned ms) { pcbkit_sleep_until(pcbkit_time_us() + ms * 1000u); }

static void lcd_start(void) {
    pcbkit_pin_output(TOUCH_CS, 1);
    pcbkit_pin_output(LCD_CS, 1);
    pcbkit_pin_output(LCD_DC, 1);
    pcbkit_pin_output(LCD_RESET, 1);
    pcbkit_pin_output(LCD_BACKLIGHT, 0);
    pcbkit_spi_init(2); /* 80 MHz / 2: the ILI9341's write cycle is 100 ns (10 MHz) by its datasheet, 40 MHz in practice */
    /* Hardware reset: low for 10 µs or more, then 120 ms before commands. */
    pcbkit_pin_put(LCD_RESET, 0);
    sleep_ms(1);
    pcbkit_pin_put(LCD_RESET, 1);
    sleep_ms(120);
    /* 16 bits a pixel; landscape: row/column exchange, BGR panel (the module's wiring). */
    static const uint8_t colmod = 0x55, madctl = 0x28;
    lcd_command(0x3a, &colmod, 1);
    lcd_command(0x36, &madctl, 1);
    lcd_command(0x11, NULL, 0); /* sleep out */
    sleep_ms(120);
    lcd_command(0x29, NULL, 0); /* display on */
    /* Black over the whole panel once, then the game's window only. */
    static const uint8_t black[64];
    lcd_window(0, 0, 319, 239);
    for (unsigned i = 0; i < 320 * 240 * 2 / sizeof black; i++) pcbkit_spi_send(-1, black, sizeof black);
    pcbkit_pin_put(LCD_CS, 1);
    pcbkit_pin_put(LCD_BACKLIGHT, 1);
}

void I_Init(void) {
    origin = pcbkit_time_us();
}

byte *I_ZoneBase(int *size) {
    *size = 5 * 1024 * 1024;
    return malloc((size_t)*size);
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
    uint32_t pressed = 0;
    for (unsigned i = 0; i < BUTTONS; i++) pressed |= (uint32_t)!pcbkit_pin_get(button_pins[i]) << i;
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

/* The four screens (256,000 bytes) in the PSRAM, behind the data cache: the SRAM keeps the
 * engine's statics and the stack. */
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
    /* RGB565 with its bytes swapped: the panel takes the high byte first. */
    for (int i = 0; i < 256; i++, palette += 3) {
        uint16_t c = (uint16_t)((palette[0] & 0xf8) << 8 | (palette[1] & 0xfc) << 3 | palette[2] >> 3);
        palette565[i] = (uint16_t)(c >> 8 | c << 8);
    }
}

void I_SetRelativeMode(boolean enabled) { (void)enabled; }
void I_MarkDirtyLines(int first, int last) { (void)first; (void)last; }
void I_UpdateNoBlit(void) {}

void I_FinishUpdate(void) {
    lcd_window(0, 20, SCREENWIDTH - 1, 20 + SCREENHEIGHT - 1);
    uint16_t chunk[32];
    const byte *src = screens[0];
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i += 32) {
        for (int k = 0; k < 32; k++) chunk[k] = palette565[src[k]];
        src += 32;
        pcbkit_spi_send(-1, (const uint8_t *)chunk, sizeof chunk);
    }
    pcbkit_pin_put(LCD_CS, 1);
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
    for (unsigned i = 0; i < BUTTONS; i++) pcbkit_pin_input(button_pins[i], 1);
    printf("Doom on doom-08 (ESP32-S3): %lu Hz, PSRAM %lu KiB\n", (unsigned long)pcbkit_sys_hz, (unsigned long)(pcbkit_psram_bytes >> 10));
    origin = pcbkit_time_us();
    D_DoomMain();
    return 0;
}

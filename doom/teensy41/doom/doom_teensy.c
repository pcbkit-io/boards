/* Doom's platform layer for doom-10: a Teensy 4.1 (i.MX RT1062, Cortex-M7 at 600 MHz) under a
 * 3.5" ILI9488 320 × 480 TFT on LPSPI4 and ten buttons. An ordinary program on the chip's devices:
 *
 *   LPSPI4 SCK pin 13 (B0_03), SDO pin 11 (B0_02); display CS pin 10, D/C pin 9, RESET pin 8,
 *   backlight pin 6; the touch controller's CS pin 5 held high
 *   buttons to ground, pulled up: forward pin 0, back 1, left 2, right 3, fire 14, use 15,
 *   strafe left 16, strafe right 17, menu 22, enter 24
 *
 * The program runs in place from the Teensy's flash; the game's data, the shareware DOOM1.WAD, is
 * linked into the flash too (the processor's `data` in the design) and read where it lies. The
 * zone heap (about 600 KiB), two screens and the engine's state share the chip's 1 MiB of OCRAM: for it the engine
 * keeps its tables in flash and its visplanes' rows in bytes, and changes screens without the melt. Each frame goes
 * through Doom's palette to 18-bit colour (the ILI9488's only colour format over SPI) and into the
 * panel's middle 320 × 200 (columns 80…399, rows 60…259 of the landscape 480 × 320), one 24-bit
 * LPSPI frame a pixel. Waiting for the next tic sleeps (WFI until SysTick). Audio and saved games
 * are absent. The engine is sysprog21/doom_riscv (GPL-2.0) at its pinned revision with those three
 * changes (tables.c/h, r_defs.h with r_plane.c, v_video.c with d_main.c). */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcbkit/imxrt1062.h>
#include <pcbkit/runtime.h>
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#define LCD_CS PAD_B0(0)
#define LCD_DC PAD_B0(11)
#define LCD_RESET PAD_B1(0)
#define LCD_BACKLIGHT PAD_B0(10)
#define TOUCH_CS PAD_EMC(8)
#define BUTTONS 10u
#define TIC_US (1000000u / TICRATE)

/* forward, back, left, right, fire, use, strafe left, strafe right, menu, enter */
static const uint8_t button_pads[BUTTONS] = {PAD_AD_B0(3), PAD_AD_B0(2), PAD_EMC(4), PAD_EMC(5), PAD_AD_B1(2), PAD_AD_B1(3), PAD_AD_B1(7), PAD_AD_B1(6), PAD_AD_B1(8), PAD_AD_B0(12)};

static uint64_t origin;
static uint32_t held;
static uint32_t palette666[256];

static void lcd_command(uint8_t command, const uint8_t *data, size_t n) {
    pcbkit_spi_bits(8);
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    pcbkit_spi_put(command);
    pcbkit_spi_wait(); /* D/C must not change under the command byte */
    pcbkit_pin_put(LCD_DC, 1);
    for (size_t i = 0; i < n; i++) pcbkit_spi_put(data[i]);
    pcbkit_spi_wait();
    pcbkit_pin_put(LCD_CS, 1);
}

static void sleep_ms(unsigned ms) { pcbkit_sleep_until(pcbkit_time_us() + ms * 1000u); }

/* RAMWR over a window, then `n` pixels of `colour`… the caller sends them (24-bit frames). */
static void lcd_window(unsigned x0, unsigned y0, unsigned x1, unsigned y1) {
    const uint8_t columns[] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    const uint8_t pages[] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_command(0x2a, columns, 4);
    lcd_command(0x2b, pages, 4);
    pcbkit_pin_put(LCD_CS, 0);
    pcbkit_pin_put(LCD_DC, 0);
    pcbkit_spi_put(0x2c);
    pcbkit_spi_wait();
    pcbkit_pin_put(LCD_DC, 1);
    pcbkit_spi_bits(24);
}

static void lcd_start(void) {
    pcbkit_pin_output(TOUCH_CS, 1);
    pcbkit_pin_output(LCD_CS, 1);
    pcbkit_pin_output(LCD_DC, 1);
    pcbkit_pin_output(LCD_RESET, 1);
    pcbkit_pin_output(LCD_BACKLIGHT, 0);
    pcbkit_spi_init(33000000, 8);
    /* Hardware reset: low for 10 µs or more, then 120 ms before commands. */
    pcbkit_pin_put(LCD_RESET, 0);
    sleep_ms(1);
    pcbkit_pin_put(LCD_RESET, 1);
    sleep_ms(120);
    /* 18 bits a pixel; landscape: row/column exchange, BGR panel (the module's wiring). */
    static const uint8_t colmod = 0x66, madctl = 0x28;
    lcd_command(0x3a, &colmod, 1);
    lcd_command(0x36, &madctl, 1);
    lcd_command(0x11, NULL, 0); /* sleep out */
    sleep_ms(120);
    lcd_command(0x29, NULL, 0); /* display on */
    /* Black over the whole panel once, then the game's window only. */
    lcd_window(0, 0, 479, 319);
    for (unsigned i = 0; i < 480 * 320; i++) pcbkit_spi_put(0);
    pcbkit_spi_wait();
    pcbkit_pin_put(LCD_CS, 1);
    pcbkit_pin_put(LCD_BACKLIGHT, 1);
}

void I_Init(void) {
    origin = pcbkit_time_us();
}

/* The zone heap: what the 1 MiB of OCRAM leaves beside the engine's state and its two screens,
 * up to 640 KiB (Doom's shareware levels need about 576 KiB: measured). */
byte *I_ZoneBase(int *size) {
    /* What the heap has left, less 64 KiB for what the engine allocates outside the zone (the
     * WAD's directory and lump cache: about 25 KiB). */
    extern char __heap_end[];
    char *top = sbrk(0);
    *size = (int)(__heap_end - top) - 64 * 1024;
    if (*size > 640 * 1024) *size = 640 * 1024;
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
    for (unsigned i = 0; i < BUTTONS; i++) pressed |= (uint32_t)!pcbkit_pin_get(button_pads[i]) << i;
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

byte *I_AllocLow(int length) { return calloc(1, (size_t)length); }
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
    for (int i = 0; i < 256; i++, palette += 3) palette666[i] = (uint32_t)(palette[0] & 0xfc) << 16 | (uint32_t)(palette[1] & 0xfc) << 8 | (palette[2] & 0xfc);
}

void I_SetRelativeMode(boolean enabled) { (void)enabled; }
void I_MarkDirtyLines(int first, int last) { (void)first; (void)last; }
void I_UpdateNoBlit(void) {}

void I_FinishUpdate(void) {
    lcd_window(80, 60, 80 + SCREENWIDTH - 1, 60 + SCREENHEIGHT - 1);
    const byte *src = screens[0];
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++) pcbkit_spi_put(palette666[*src++]);
    pcbkit_spi_wait();
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
    for (unsigned i = 0; i < BUTTONS; i++) pcbkit_pin_input(button_pads[i], 1);
    origin = pcbkit_time_us();
    D_DoomMain();
    return 0;
}

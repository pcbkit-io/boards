/* Doom's platform layer for doom-09: an ESP32-S3-WROOM-1-N16R8 scanning a raw 4.3" 480 × 272 RGB
 * TFT (Newhaven NHD-4.3-480272EF-ASXP) with LCD_CAM from a frame buffer in its octal PSRAM, and
 * ten buttons on an MCP23017. An ordinary program on the chip's devices:
 *
 *   RGB565 on 16 lines: B3–B7 IO9–IO13, G2–G7 IO7 IO15 IO16 IO17 IO18 IO8, R3–R7 IO1 IO2 IO4 IO5
 *   IO6; PCLK IO42, DE IO40, HSYNC IO39, VSYNC IO41; the backlight's boost enabled on IO46
 *   the MCP23017 at 0x20 on I2C0 (SDA IO14, SCL IO38): forward GPA0, back GPA1, left GPA2,
 *   right GPA3, fire GPA4, use GPA5, strafe left GPA6, strafe right GPB0, menu GPB1, enter GPB2,
 *   each a button to ground on the expander's pull-up
 *
 * The program and the game's data, the shareware DOOM1.WAD (the processor's `data` in the
 * design), run and are read in place from flash through the caches; the zone heap, the screens,
 * the visplanes and the panel's frame are in the PSRAM. LCD_CAM sends the frame by GDMA without
 * end at 8.9 MHz (80 MHz / 9; the panel's typical timing in SYNC-DE mode: HSYNC 4 + 39 + 480 + 8
 * = 531 clocks, 59.7 us a line, VSYNC 4 + 8 + 272 + 8 = 292 lines: 57 Hz);
 * each Doom frame is scaled 1.36 times through the palette into its middle 435 × 272 and written
 * back from the data cache for the DMA to read. Waiting for the next tic sleeps (WAITI until
 * CCOMPARE0). Audio and saved games are absent. The engine is sysprog21/doom_riscv (GPL-2.0) at
 * its pinned revision, its tables const (tables.c/h, r_main.c: in flash) and two arrays in the
 * PSRAM (r_plane.c, w_wad.c). */
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

#define PANEL_W 480
#define PANEL_H 272
#define VIEW_W 435
#define VIEW_X ((PANEL_W - VIEW_W) / 2)
#define BACKLIGHT 46
#define KEYS 0x20
#define BUTTONS 10u
#define TIC_US (1000000u / TICRATE)

static const struct pcbkit_rgb panel = {
    .pclk = 42, .de = 40, .hsync = 39, .vsync = 41, .pclk_neg = 0,
    .data = {9, 10, 11, 12, 13, 7, 15, 16, 17, 18, 8, 1, 2, 4, 5, 6},
    .pclk_hz = 9000000, .width = PANEL_W, .height = PANEL_H,
    .hsw = 4, .hbp = 39, .hfp = 8, .vsw = 4, .vbp = 8, .vfp = 8,
};

static PCBKIT_PSRAM uint16_t frame[PANEL_W * PANEL_H] __attribute__((aligned(64)));
static uint16_t palette565[256];
static uint16_t column_of[VIEW_W];
static uint8_t row_of[PANEL_H];
static uint64_t origin;
static uint32_t held;

static void expander(uint8_t reg, uint8_t a, uint8_t b) {
    const uint8_t bytes[] = {reg, a, b};
    pcbkit_i2c_transfer(KEYS, bytes, 3, NULL, 0);
}

void I_Init(void) { origin = pcbkit_time_us(); }

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

/* The buttons, read from the expander's ports each tic (pressed: low). */
void I_StartTic(void) {
    static const int keys[BUTTONS] = {KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW, KEY_RCTRL, ' ', ',', '.', KEY_ESCAPE, KEY_ENTER};
    uint8_t reg = 0x12, ports[2];
    if (pcbkit_i2c_transfer(KEYS, &reg, 1, ports, 2) != 0) return;
    uint32_t pressed = (~(uint32_t)ports[0] & 0x7fu) | (~(uint32_t)ports[1] & 0x07u) << 7;
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

/* The four screens (256,000 bytes) in the PSRAM, behind the data cache. */
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
    for (unsigned x = 0; x < VIEW_W; x++) column_of[x] = (uint16_t)(x * SCREENWIDTH / VIEW_W);
    for (unsigned y = 0; y < PANEL_H; y++) row_of[y] = (uint8_t)(y * SCREENHEIGHT / PANEL_H);
    memset(frame, 0, sizeof frame);
    if (pcbkit_rgb_start(&panel, frame) != 0) I_Error("RGB panel: too many DMA descriptors");
    pcbkit_pin_output(BACKLIGHT, 1);
}
void I_ShutdownGraphics(void) {}

void I_SetPalette(byte *palette) {
    for (int i = 0; i < 256; i++, palette += 3) palette565[i] = (uint16_t)((palette[0] & 0xf8) << 8 | (palette[1] & 0xfc) << 3 | palette[2] >> 3);
}

void I_SetRelativeMode(boolean enabled) { (void)enabled; }
void I_MarkDirtyLines(int first, int last) { (void)first; (void)last; }
void I_UpdateNoBlit(void) {}

/* The panel lies turned half a turn (its cable side, the bottom of its own scan, at the board's top
 * edge): the frame goes out from its last pixel to its first. */
void I_FinishUpdate(void) {
    for (unsigned y = 0; y < PANEL_H; y++) {
        const byte *src = screens[0] + row_of[y] * SCREENWIDTH;
        uint16_t *dst = frame + (PANEL_H - 1 - y) * PANEL_W + (PANEL_W - 1 - VIEW_X);
        for (unsigned x = 0; x < VIEW_W; x++) dst[-(int)x] = palette565[src[column_of[x]]];
    }
    pcbkit_cache_writeback(frame, sizeof frame);
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
    pcbkit_i2c_init(14, 38, 400000);
    expander(0x0c, 0x7f, 0x07); /* GPPUA, GPPUB: pull-ups on the buttons' pins */
    printf("Doom on doom-09 (ESP32-S3, RGB panel): %lu Hz, PSRAM %lu KiB\n", (unsigned long)pcbkit_sys_hz, (unsigned long)(pcbkit_psram_bytes >> 10));
    origin = pcbkit_time_us();
    D_DoomMain();
    return 0;
}

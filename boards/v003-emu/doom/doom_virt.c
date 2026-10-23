/* Doom's platform layer for pcbkit-virt, the RISC-V computer the CH32V003 builds out of its
 * serial chips (../emu): the 8-bit screen goes to the frame-buffer display controller with Doom's
 * palette as its colour table, the eight buttons are GPIO 0…7 (pulled up, low while pressed),
 * time is the CLINT's 10 MHz mtime. The shareware DOOM1.WAD is linked into the program (the
 * flash's `data` in the design) and read where it lies; the zone heap is in RAM (the PSRAM).
 * The engine is sysprog21/doom_riscv (GPL-2.0), unchanged at its pinned revision. */
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcbkit/virt.h>
#include <pcbkit/runtime.h>
#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#define BUTTONS 8u
#define TIC (TIMEBASE_HZ / TICRATE)

static uint64_t origin;
static uint32_t held;

void I_Init(void) { origin = pcbkit_mtime(); }

byte *I_ZoneBase(int *size) {
    *size = 2 * 1024 * 1024;
    return malloc(*size);
}

int I_GetTime(void) { return (int)((pcbkit_mtime() - origin) / TIC); }

void I_StartFrame(void) {}

/* Up, down, left, right; fire (ctrl, and enter for the menus); use; strafe (alt); menu (escape). */
static void post(int type, int key) {
    event_t event = {.type = type, .data1 = key};
    D_PostEvent(&event);
}

void I_StartTic(void) {
    static const int keys[BUTTONS] = {KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW, KEY_RCTRL, ' ', KEY_RALT, KEY_ESCAPE};
    uint32_t pressed = ~GPIO_INPUT_VAL & ((1u << BUTTONS) - 1);
    uint32_t changed = pressed ^ held;
    held = pressed;
    for (unsigned i = 0; i < BUTTONS; i++) {
        if (!(changed & (1u << i))) continue;
        int type = pressed & (1u << i) ? ev_keydown : ev_keyup;
        post(type, keys[i]);
        if (i == 4) post(type, KEY_ENTER);
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
    FB_ADDR = (uint32_t)(uintptr_t)screens[0];
    FB_WIDTH = SCREENWIDTH;
    FB_HEIGHT = SCREENHEIGHT;
    FB_STRIDE = SCREENWIDTH;
    FB_FORMAT = FB_FORMAT_L8;
    FB_CTRL = 1;
}
void I_ShutdownGraphics(void) {}

void I_SetPalette(byte *palette) {
    for (int i = 0; i < 256; i++, palette += 3) FB_CLUT(i) = (uint32_t)palette[0] << 16 | (uint32_t)palette[1] << 8 | palette[2];
}

void I_SetRelativeMode(boolean enabled) { (void)enabled; }
void I_MarkDirtyLines(int first, int last) { (void)first; (void)last; }
void I_UpdateNoBlit(void) {}
void I_FinishUpdate(void) { FB_PRESENT = 1; }
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
    GPIO_INPUT_EN = (1u << BUTTONS) - 1;
    GPIO_PUE = (1u << BUTTONS) - 1;
    origin = pcbkit_mtime();
    D_DoomMain();
    return 0;
}

#include "config.h" // PCBKit: build settings, before anything tests them
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
// Copyright(C) 2026 PCBKit
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	The system layer of rp2040-doom's engine on a PCBKit board, in place of its src/pico
//	files: main, the zone heap over the board's RAM (malloc included, as rp2040-doom does on
//	the device), time, the board's buttons as key events, no sound or music (their modules
//	stand in, the sound module's update drives the display), no network, no saving to flash.
//
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico.h"
#include "config.h"
#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "z_zone.h"
#include "piconet.h"
#include "doom_platform.h"

void D_DoomMain(void);
void pcbkit_doom_display(void);

int main(void) {
    I_Init();
    D_DoomMain();
    return 0;
}

void I_Init(void) { pcbkit_doom_board_init(); }

// ---- the zone heap, which malloc uses too ------------------------------------------------------

byte *I_ZoneBase(int *size) {
    uint8_t *base;
    size_t bytes;
    pcbkit_doom_heap(&base, &bytes);
    // 16-bit pointers (shortptr_t) reach 256 KiB from SHORTPTR_BASE.
    uintptr_t limit = SHORTPTR_BASE + 0x40000;
    if ((uintptr_t)base + bytes > limit) bytes = limit - (uintptr_t)base;
    *size = (int)bytes;
    printf("zone memory: %p, %x allocated for zone\n", base, *size);
    return base;
}

static boolean zone_ready;
static void zone(void) {
    if (!zone_ready) {
        zone_ready = true;
        Z_Init();
    }
}
void *malloc(size_t size) {
    zone();
    return Z_MallocNoUser((int)size, PU_STATIC);
}
void *calloc(size_t count, size_t size) {
    void *p = malloc(count * size);
    memset(p, 0, count * size);
    return p;
}
void free(void *p) {
    if (p) Z_Free(p);
}
void *realloc(void *p, size_t size) {
    void *q = malloc(size);
    if (p) {
        memcpy(q, p, size); // the zone's blocks are at least as large as asked; reading past a smaller one stays in the zone
        free(p);
    }
    return q;
}

// ---- failures ----------------------------------------------------------------------------------

void panic(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    putchar('\n');
    exit(1);
}
void panic_unsupported(void) { panic("not supported"); }
void I_Quit(void) { exit(0); }
void I_Tactile(int on, int off, int total) {}

// ---- banners -----------------------------------------------------------------------------------

void I_PrintBanner(const char *msg) { puts(msg); }
void I_PrintDivider(void) { puts("==========================================================================="); }
void I_PrintStartupBanner(const char *gamedescription) {
    I_PrintDivider();
    I_PrintBanner(gamedescription);
    I_PrintDivider();
    puts(" " PACKAGE_NAME " is free software, covered by the GNU General Public License.");
    I_PrintDivider();
}

// ---- time --------------------------------------------------------------------------------------

void I_InitTimer(void) {}
int I_GetTimeMS(void) { return (int)(pcbkit_doom_time_us() / 1000); }
int I_GetTime(void) { return (int)(pcbkit_doom_time_us() * TICRATE / 1000000); }
void I_Sleep(int ms) { pcbkit_doom_sleep_until(pcbkit_doom_time_us() + (uint64_t)ms * 1000); }
void I_WaitVBL(int count) { I_Sleep(count * 1000 / 70); }

// ---- input: the board's buttons ---------------------------------------------------------------

void I_BindInputVariables(void) {}
void I_StartTextInput(int x1, int y1, int x2, int y2) {}
void I_StopTextInput(void) {}

void I_StartTic(void) {
    static const int keys[PCBKIT_DOOM_KEYS] = {KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW, KEY_RCTRL, ' ', ',', '.', KEY_ESCAPE, KEY_ENTER, '\t'};
    static uint32_t held;
    pcbkit_doom_display();
    uint32_t pressed = pcbkit_doom_buttons(), changed = pressed ^ held;
    held = pressed;
    for (int i = 0; i < PCBKIT_DOOM_KEYS; i++) {
        if (changed >> i & 1) {
            event_t event = {.type = pressed >> i & 1 ? ev_keydown : ev_keyup, .data1 = keys[i], .data2 = keys[i]};
            D_PostEvent(&event);
        }
    }
}

// ---- no sound or music: the sound module's update shows finished frames -------------------------

static boolean pcbkit_sound_init(boolean use_sfx_prefix) { return true; }
static void pcbkit_sound_shutdown(void) {}
static int pcbkit_sfx_lump(should_be_const sfxinfo_t *sfx) { return 0; }
static void pcbkit_sound_params(int channel, int vol, int sep) {}
static int pcbkit_start_sound(should_be_const sfxinfo_t *sfx, int channel, int vol, int sep, int pitch) { return channel; }
static void pcbkit_stop_sound(int channel) {}
static boolean pcbkit_sound_playing(int channel) { return false; }
static void pcbkit_cache_sounds(should_be_const sfxinfo_t *sounds, int count) {}
sound_module_t sound_pico_module = {
    .sound_devices = NULL, .num_sound_devices = 0,
    .Init = pcbkit_sound_init, .Shutdown = pcbkit_sound_shutdown, .GetSfxLumpNum = pcbkit_sfx_lump,
    .Update = pcbkit_doom_display, .UpdateSoundParams = pcbkit_sound_params, .StartSound = pcbkit_start_sound,
    .StopSound = pcbkit_stop_sound, .SoundIsPlaying = pcbkit_sound_playing, .CacheSounds = pcbkit_cache_sounds,
};
static boolean pcbkit_music_init(void) { return true; }
static void pcbkit_music_none(void) {}
static void pcbkit_music_volume(int volume) {}
static void *pcbkit_music_register(should_be_const void *data, int len) { return NULL; }
static void pcbkit_music_unregister(void *handle) {}
static void pcbkit_music_play(void *handle, boolean looping) {}
static boolean pcbkit_music_playing(void) { return false; }
const music_module_t music_opl_module = {
    .sound_devices = NULL, .num_sound_devices = 0, .Init = pcbkit_music_init, .Shutdown = pcbkit_music_none,
    .SetMusicVolume = pcbkit_music_volume, .PauseMusic = pcbkit_music_none, .ResumeMusic = pcbkit_music_none,
    .RegisterSong = pcbkit_music_register, .UnRegisterSong = pcbkit_music_unregister, .PlaySong = pcbkit_music_play,
    .StopSong = pcbkit_music_none, .MusicIsPlaying = pcbkit_music_playing, .Poll = NULL,
};
void I_SetOPLDriverVer(opl_driver_ver_t ver) {}
void I_PicoSoundFade(boolean in) {}
boolean I_PicoSoundFading(void) { return false; }
uint8_t restart_song_state;

// ---- no network, no saving to flash ------------------------------------------------------------

boolean drone = false;
boolean net_client_connected = false;
void piconet_init() {}
void piconet_start_host(int8_t deathmatch, int8_t epi, int8_t skill) {}
void piconet_start_client() {}
void piconet_stop() {}
bool piconet_client_check_for_dropped_connection() { return true; }
void piconet_start_game() {}
int piconet_get_lobby_state(lobby_state_t *state) { memset(state, 0, sizeof *state); return -1; }
void piconet_new_local_tic(int tic) {}
int piconet_maybe_recv_tic(int fromtic) { return fromtic; }
void picoflash_sector_program(uint32_t flash_offs, const uint8_t *data) {}

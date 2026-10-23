/* PCBKit: what rp2040-doom's engine needs from a board (the board's own doom_<board>.c gives it).
 * Everything else (the zone heap, video composition, input events, the absent sound and network)
 * is the shared platform layer in i_system.c and i_video.c. */
#ifndef PCBKIT_DOOM_PLATFORM_H
#define PCBKIT_DOOM_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint64_t pcbkit_doom_time_us(void);
void pcbkit_doom_sleep_until(uint64_t us);
/* The board's start-up (clocks, display, buttons' pins), before the engine starts. */
void pcbkit_doom_board_init(void);
/* The heap the engine's zone takes: at most 256 KiB from SHORTPTR_BASE (16-bit pointers). */
void pcbkit_doom_heap(uint8_t **base, size_t *size);
/* The buttons held now, a bit each: PCBKIT_DOOM_UP … PCBKIT_DOOM_ENTER. */
uint32_t pcbkit_doom_buttons(void);
enum { PCBKIT_DOOM_UP, PCBKIT_DOOM_DOWN, PCBKIT_DOOM_LEFT, PCBKIT_DOOM_RIGHT, PCBKIT_DOOM_FIRE, PCBKIT_DOOM_USE,
       PCBKIT_DOOM_STRAFE_LEFT, PCBKIT_DOOM_STRAFE_RIGHT, PCBKIT_DOOM_MENU, PCBKIT_DOOM_ENTER, PCBKIT_DOOM_WEAPON,
       PCBKIT_DOOM_KEYS };
/* A composed 320 × 200 frame, a line of RGB565 at a time (y = 0 … 199), then its end. */
void pcbkit_doom_line(int y, const uint16_t *rgb565);
void pcbkit_doom_frame_end(void);
/* The WHX (rp2040-doom's compressed DOOM1.WAD), linked from the design's data file DOOM1.WHX. */
extern const uint8_t pcbkit_data_DOOM1_WHX[];
#ifdef __cplusplus
}
#endif
#endif

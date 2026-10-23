#pragma once
/* PCBKit: Pico SDK time functions on the machine's microsecond clock (see pico.h). */
#ifndef PCBKIT_PICO_TIME_H
#define PCBKIT_PICO_TIME_H
#include "pico.h"
#include "doom_platform.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uint64_t absolute_time_t;
static inline uint64_t time_us_64(void) { return pcbkit_doom_time_us(); }
static inline uint32_t time_us_32(void) { return (uint32_t)pcbkit_doom_time_us(); }
static inline absolute_time_t get_absolute_time(void) { return time_us_64(); }
static inline uint64_t to_us_since_boot(absolute_time_t t) { return t; }
static inline void sleep_us(uint64_t us) { pcbkit_doom_sleep_until(time_us_64() + us); }
static inline void sleep_ms(uint32_t ms) { sleep_us((uint64_t)ms * 1000); }
#ifdef __cplusplus
}
#endif
#endif

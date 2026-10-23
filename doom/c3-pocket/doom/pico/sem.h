#pragma once
/* PCBKit: one core runs everything; a semaphore only counts (see pico.h). */
#ifndef PCBKIT_PICO_SEM_H
#define PCBKIT_PICO_SEM_H
#include "pico.h"
#include "pico/sync.h"
typedef struct { int16_t permits, max; } semaphore_t;
static inline void sem_init(semaphore_t *s, int16_t initial, int16_t max) { s->permits = initial; s->max = max; }
static inline bool sem_release(semaphore_t *s) { if (s->permits < s->max) { s->permits++; return true; } return false; }
static inline bool sem_try_acquire(semaphore_t *s) { if (s->permits > 0) { s->permits--; return true; } return false; }
static inline void sem_acquire_blocking(semaphore_t *s) { if (s->permits > 0) s->permits--; }
static inline int sem_available(semaphore_t *s) { return s->permits; }
static inline bool sem_acquire_timeout_ms(semaphore_t *s, uint32_t ms) { (void)ms; return sem_try_acquire(s); }
#endif

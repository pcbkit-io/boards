#pragma once
/* PCBKit: an empty stand-in for the Pico SDK header (see pico.h). */
#include "pico.h"
typedef struct { int locked; } spin_lock_t;
static inline spin_lock_t *spin_lock_instance(uint n) { static spin_lock_t locks[32]; return &locks[n & 31]; }
static inline uint32_t spin_lock_blocking(spin_lock_t *l) { l->locked = 1; return 0; }
static inline void spin_unlock(spin_lock_t *l, uint32_t saved) { (void)saved; l->locked = 0; }

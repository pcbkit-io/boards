#pragma once
/* PCBKit: an empty stand-in for the Pico SDK header (see pico.h). */
#include "pico.h"
static inline uint32_t save_and_disable_interrupts(void) { return 0; }
static inline void restore_interrupts(uint32_t status) { (void)status; }

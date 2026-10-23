#pragma once
/* PCBKit: the RP2040 divider's API in plain C (see pico.h). */
#ifndef PCBKIT_PICO_DIVIDER_H
#define PCBKIT_PICO_DIVIDER_H
#include "pico.h"
static inline int32_t div_s32s32(int32_t a, int32_t b) { return b ? a / b : (a < 0 ? 1 : -1); }
static inline uint32_t div_u32u32(uint32_t a, uint32_t b) { return b ? a / b : 0xffffffffu; }
static inline int32_t div_s32s32_unsafe(int32_t a, int32_t b) { return div_s32s32(a, b); }
static inline uint32_t div_u32u32_unsafe(uint32_t a, uint32_t b) { return div_u32u32(a, b); }
static inline int64_t div_s64s64(int64_t a, int64_t b) { return b ? a / b : (a < 0 ? 1 : -1); }
static inline uint64_t div_u64u64(uint64_t a, uint64_t b) { return b ? a / b : ~0ull; }
#endif
static inline uint32_t hw_divider_u32_quotient_inlined(uint32_t a, uint32_t b) { return div_u32u32(a, b); }
static inline int32_t hw_divider_s32_quotient_inlined(int32_t a, int32_t b) { return div_s32s32(a, b); }

/* PCBKit: the parts of the Raspberry Pi Pico SDK's pico.h that rp2040-doom's engine uses, for
 * running it on any machine (written for this port; no SDK code). */
#ifndef PCBKIT_PICO_H
#define PCBKIT_PICO_H
#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned int uint;
#define __not_in_flash(group)
#define __not_in_flash_func(f) f
#define __no_inline_not_in_flash_func(f) __attribute__((noinline)) f
#define __time_critical_func(f) f
#define __in_flash(group)
#define __scratch_x(group)
#define __scratch_y(group)
#define __uninitialized_ram(name) name
#ifndef __aligned
#define __aligned(n) __attribute__((aligned(n)))
#endif
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#ifndef __noinline
#define __noinline __attribute__((__noinline__))
#endif
#define __force_inline inline __attribute__((always_inline))
#define __fast_mul(a, b) ((a) * (b))
#define __mul_instruction(a, b) ((a) * (b))
#ifndef count_of
#define count_of(a) (sizeof(a)/sizeof((a)[0]))
#endif
#define __XSTRING(x) __STRING(x)
#ifndef __STRING
#define __STRING(x) #x
#endif
#define hard_assert(x) assert(x)
#define invalid_params_if(group, x) ((void)0)
#define valid_params_if(group, x) ((void)0)
#define bi_decl(x)
#define bi_decl_if_func_used(x)
void panic(const char *format, ...) __attribute__((noreturn));
void panic_unsupported(void) __attribute__((noreturn));
static inline uint get_core_num(void) { return 0; }
static inline void tight_loop_contents(void) {}
static inline void __breakpoint(void) { __builtin_trap(); }
#ifdef __cplusplus
}
#endif
#endif

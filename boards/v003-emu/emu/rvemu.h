/* rvemu: a RISC-V computer (RV32IMAC, machine mode) emulated by a small microcontroller whose
 * memories are serial chips: the guest's program is an ELF file stored from the start of a SPI
 * NOR flash, its RAM is a SPI PSRAM. The guest sees the devices of PCBKit's pcbkit-virt computer
 * (RAM at 0x80000000, CLINT, NS16550A UART, SiFive GPIO, the frame-buffer display controller,
 * the test syscon), so a program built for pcbkit-virt runs here unchanged.
 *
 * The host (the chip's own program) provides the memories line by line, the time, the buttons
 * and the screen; this file is the processor, the cache and the devices. */
#ifndef RVEMU_H
#define RVEMU_H
#include <stdint.h>

/* Bytes per cache line: one memory transaction. */
#define RV_LINE 16u

/* What the host provides. A line address is a byte offset in the flash (`flash` true) or the
 * PSRAM, a multiple of RV_LINE. */
void rv_host_read(int flash, uint32_t offset, uint8_t *line);
void rv_host_write(uint32_t offset, const uint8_t *line);
/* Microseconds since the host started. */
uint64_t rv_host_time_us(void);
/* The guest's GPIO input pins (bit n: GPIO n), as the board's buttons hold them. */
uint32_t rv_host_inputs(void);
/* A frame for the screen: `width` × `height` 8-bit pixels at guest address `address`, `stride`
 * bytes a row; `luma` the 4-bit brightness of each colour index. The host reads the pixels with
 * rv_read. */
void rv_host_present(uint32_t address, uint32_t width, uint32_t height, uint32_t stride, const uint8_t *luma);
/* A byte the guest wrote to its console. */
void rv_host_console(uint8_t byte);
/* The guest stopped: powered off (`code` 0), failed (its code), or did something this computer
 * does not provide (`why`). Does not return. */
void rv_host_stop(const char *why, uint32_t code, uint32_t pc);

/* Load the guest from the ELF at the start of the flash (`ram_bytes` of PSRAM) and run it. */
void rv_run(uint32_t ram_bytes);
/* Guest memory, for the host's frame reading: `n` bytes from guest address `address`. */
void rv_read(uint32_t address, uint8_t *to, uint32_t n);
#endif

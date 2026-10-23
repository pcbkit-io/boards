/* The STM32F411's 128 KiB of SRAM holds a 64 KiB cache: 2048 lines of 32 bytes (one transaction
 * is the command, the address, a dummy byte and 32 bytes: 5.9 µs at 50 MHz, inside the PSRAM's
 * 8 µs limit with its chip selected). */
#define RV_LINE 32u
#define RV_LINES 2048u

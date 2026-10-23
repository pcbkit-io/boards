#pragma once
/* pico-oled: the RP2040's SRAM from 0x2000_0000; .data, .bss and the zone heap fit its first
 * 256 KiB (16-bit pointers reach that far), the stack lives in the 8 KiB above. */
#define SHORTPTR_BASE 0x20000000

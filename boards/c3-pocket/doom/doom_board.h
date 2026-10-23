#pragma once
/* c3-pocket: the ESP32-C3's DRAM from 0x3FC9_0000 (above the IRAM's 64 KiB of the same SRAM);
 * .data, .bss and the zone heap fit its first 256 KiB (16-bit pointers reach that far), the stack
 * lives at its top. */
#define SHORTPTR_BASE 0x3FC90000

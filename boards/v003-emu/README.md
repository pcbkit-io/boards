# 01 · v003-emu — Doom on a 10-cent RISC-V, by emulation

The most primitive variant: a WCH CH32V003 cannot hold Doom (2 KB of RAM), so it does not try.
Its firmware is a small RV32IMA interpreter; the real program — Doom built for a plain RV32 machine
— lives in an 8 MB SPI PSRAM and boots from a 16 MB SPI flash. It is the "Linux on an 8-bit AVR"
trick applied to Doom: slow (well under one frame per second, estimate below), but every part is a
real, orderable chip, and it is the clearest test of whether PCBKit's engine models *buses* rather
than *programs*.

| | |
|---|---|
| Processor | WCH CH32V003F4P6, QingKe V2A core: RISC-V **RV32EC** (16 registers, compressed) + WCH's XW byte/halfword extension, 48 MHz (24 MHz HSI × 2 PLL), 2 KB SRAM, 16 KB flash, TSSOP-20 |
| Memory | APS6404L-3SQR 8 MB PSRAM (guest RAM) and W25Q128JV 16 MB NOR flash (guest kernel + WAD), both on SPI1 (PC5/PC6/PC7) with their own chip selects (PC0, PC4) |
| Display | Nokia 5110 module, PCD8544, 84 × 48 monochrome, SPI shared with the memories (CS PC3, D/C PC2), reset by RC at power-up |
| Input | 8 tact switches (5.1 mm) to GND, internal pull-ups: PD0, PD2–PD6, PA1, PA2 (the crystal pins are free: the HSI runs the chip) |
| Sound | passive piezo on PC1 (TIM2 CH4 remap) |
| Power | 2 × AA alkaline → slide switch on the boost's EN → TPS61023 synchronous boost to 3.3 V (453 k / 100 k on FB: 3.29 V) |
| Programming | WCH single-wire debug (SDI) on PD1: 3-pin header for a WCH-LinkE |
| Board | 96 × 50 mm, 2 layers; front: LCD, buttons, switch; back: everything else |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 0 warnings, 0 unrouted nets; 36 components, 28 nets, 287 track segments (681 mm), 29 vias.
<!-- /STATUS -->

## Why it can (barely) run Doom

The CH32V003's job is only to be a CPU for an emulated one:

1. Boot: the V003's own firmware (≈ 6 KB, in its 16 KB flash) is an RV32IMA interpreter with a
   direct-mapped cache of PSRAM lines in its SRAM (e.g. 32 lines × 32 bytes = 1 KB).
2. It copies the guest image (Doom for a bare RV32 machine with a tiny platform layer: framebuffer
   at a fixed address, buttons and timer as MMIO) from the W25Q128 into the PSRAM, then runs it.
3. Guest loads/stores go through the cache; misses are SPI transactions to the PSRAM
   (03h read / 02h write, 24 MHz SCK: ~13 µs for a 32-byte line incl. command and address).
4. Every few hundred milliseconds the interpreter reads the guest's 320 × 200 8-bit frame,
   downsamples it to 84 × 48, dithers it to 1 bit and sends 504 bytes to the PCD8544.

**Speed (estimate, not measured on silicon).** PCBKit's own Doom on `pcbkit-virt` needs about
1.1 M guest instructions per frame (measured: 354 M instructions for 324 frames,
`research/machine-framework-2026-10-23.md`). An interpreter on a 48 MHz RV32EC core costs roughly
30–60 host cycles per guest instruction plus cache misses, i.e. 0.5–1 guest MIPS: **about one frame
every 1–2 seconds**. It is a demonstration board, not a game console — and the reason it is
variant 01.

## Power

| State | Current at 3.3 V (estimate) |
|---|---|
| CH32V003 at 48 MHz | ~5 mA |
| PSRAM active (SPI, low clock) | ~3–5 mA (datasheet: 5.5 mA typ at 133 MHz) |
| W25Q128 (only while booting) | ~10 mA |
| PCD8544 + backlight through 330 Ω | ~0.3 mA + ~4 mA |
| **Total** | **~15 mA ≈ 50 mW** |

From two alkaline AA cells (≈ 2.5 Wh usable at this drain) through the boost at ~85 %: **~40 h**.
The TPS61023 starts from 1.8 V and keeps running down to ~0.5 V input, so the cells are emptied.

## Storage of the game data

The guest image (emulated kernel-less Doom + its WAD) is written to the W25Q128 once with any
SPI flash programmer (CH341A clip) or by the V003 firmware itself receiving it over the SDI debug
link. 16 MB holds the shareware DOOM1.WAD (4.2 MB) uncompressed.

## Firmware: build and flash

- Toolchain: `riscv-none-elf-gcc` (xPack) with `-march=rv32ec_zicsr -mabi=ilp32e`, or the
  open-source **ch32v003fun** SDK (MIT), which is small enough for an interpreter.
- Flash: `minichlink -w emu.bin flash -b` (ch32v003fun) or WCH-LinkUtility, through a **WCH-LinkE**
  on J2 (3V3, SWIO = PD1, GND).
- The guest: built with `riscv32-unknown-elf-gcc -march=rv32ima`, written to the flash at
  offset 0 (`flashrom -p ch341a_spi -w guest.bin`).

## Bill of materials

Key parts and a rough cost at 10 pieces (estimates, LCSC/AliExpress prices of 2026):
CH32V003F4P6 $0.15 · APS6404L $1.20 · W25Q128JV $1.00 · Nokia 5110 module $2.50 · TPS61023 $0.60 ·
JS102011SAQN $0.30 · buttons 8 × $0.05 · passives $0.50 · 2×AA holder $0.50 · PCB (2-layer, 96 × 50)
$1.00 → **about $8 per unit** plus two AA cells.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 1 | BT1 |  | 2xAA battery holder with leads | BatteryLeads_2x_D1.1mm_P3.81mm |
| 1 | BZ1 |  | GPC12075YB-5V4000 | Buzzer_D12.0mm_H7.5mm_P6.50mm |
| 5 | C1, C2, C5, C7, C8 | 100nF |  | C_0402_1005Metric |
| 1 | C3 | 10uF |  | C_0603_1608Metric |
| 1 | C4 | 22uF |  | C_0805_2012Metric |
| 1 | C6 | 1uF |  | C_0402_1005Metric |
| 1 | DS1 |  | Nokia 5110 84x48 LCD module (PCD8544): connector (8-pin) | PinHeader_1x08_P2.54mm_Vertical |
| 1 | J2 | PinHeader_1x03_P2.54mm_Vertical |  | PinHeader_1x03_P2.54mm_Vertical |
| 1 | L1 | 1uH |  | L_Power_4x4mm |
| 6 | R1, R2, R7, R8, R9, R10 | 10kohm |  | R_0402_1005Metric |
| 1 | R3 | 330ohm |  | R_0603_1608Metric |
| 1 | R4 | 1Mohm |  | R_0402_1005Metric |
| 1 | R5 | 453kohm |  | R_0402_1005Metric |
| 1 | R6 | 100kohm |  | R_0402_1005Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | SW_Tactile_SPST_5.1x5.1mm_SMD |  | SW_Tactile_SPST_5.1x5.1mm_SMD |
| 1 | SW9 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | U1 |  | CH32V003F4P6 | TSSOP-20_4.4x6.5mm_P0.65mm |
| 1 | U2 |  | APS6404L-3SQR-SN | SOIC-8_3.9x4.9mm_P1.27mm |
| 1 | U3 |  | W25Q128JVSIQ | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U4 |  | TPS61023DRLR | SOT-563 |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** RV32EC (16 registers; the current RV32IMAC core with a register-count switch and E-ABI
  traps on x16–x31) plus WCH's XW extension (compressed byte/halfword loads and stores) and the
  V2A's lack of M (multiply in software).
- **Interrupt controller:** WCH PFIC (a CLIC-like controller with vector table, hardware stacking
  "HPE" and nesting), SysTick in the core's address space.
- **Boot:** the V003 boots from user flash at 0x0000_0000 (a boot area with WCH's bootloader is
  optional); option bytes; no ROM image format.
- **Clocks:** HSI 24 MHz ×2 PLL, AHB prescaler; a cycle model matters here (the speed estimate is
  all about cycles per guest instruction and SPI time).
- **Peripherals the firmware uses:** SPI1 (mode 0, 8-bit, polling), GPIO (GPIOA/C/D with CFGLR
  4-bit mode fields), AFIO remap (PC1 TIM2), TIM2 PWM, the SDI debug transport for flashing.
- **External devices on SPI with chip selects:** an APS6404L model (03h/0Bh/02h/38h/EBh commands,
  QPI enter/exit, 8 MB array) and a W25Q128 model (read, page program, sector erase, status), plus
  a PCD8544 display model (D/C pin, 84 × 48 RAM, addressing modes). The current `Link::spi` moves
  one byte without a chip select: these devices need **CS and D/C framing** on the link.
- **Board:** TPS61023 as a voltage source with efficiency, two AA cells as a discharge curve.

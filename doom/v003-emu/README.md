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
| Power | 2 × 18650 Li-ion in parallel (1S2P) behind reverse-cell (AO3401A) and DW01A + FS8205A protection; BQ24075 charger with power path from a USB-C port (fuse, TVS); SYS → AP2112K-3.3 LDO, its enable on the slide switch |
| Programming | WCH single-wire debug (SDI) on PD1: J2, a 3-pin header (3V3, SWIO, GND) at the right edge of the back for a WCH-LinkE |
| Board | 96 × 92 mm, 2 layers, four M3 holes; front: LCD, buttons, USB-C, charger, switch, test pads; back: two 18650 holders, the processor and its memories, piezo, J2 |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 0 warnings, 0 unrouted nets. 67 components, 46 nets, 439 track segments (1295 mm), 38 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partial. Nothing to fix, 1 to consider, 15 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
<!-- /STATUS -->

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/doom/v003-emu, open the
Laboratory and press **Build & run**: the page builds both programs — `emu/` for the CH32V003
(RV32EC) and `doom/` for the computer it emulates (pcbkit-virt, with `DOOM1.WAD` linked in) — puts
the second in the W25Q128 and runs the board. Until the first frame the LCD shows Doom's start-up
messages; the title screen comes after about three minutes of device time, then the demo.

## Why it can (barely) run Doom

The CH32V003's job is only to be a CPU for an emulated one (`emu/`):

1. Its firmware (15.6 KB of its 16 KB flash) is an RV32IMAC interpreter (`emu/rvemu.c`) with a
   direct-mapped write-back cache of 32 lines × 16 bytes in its 2 KB of SRAM.
2. The guest (`doom/`: Doom for pcbkit-virt, an ELF file) is in the W25Q128: its code and
   constants, the WAD linked into them, are read where they lie; its writable segment is copied
   into the PSRAM at boot.
3. Guest loads and stores go through the cache; a miss is one SPI transaction (command, 24-bit
   address and 16 bytes at 24 MHz, 6.7 µs with the chip selected: inside the PSRAM's 8 µs limit).
4. Each guest frame (320 × 200, 8-bit) is scaled to 84 × 48 with ordered dithering and sent to
   the PCD8544; its start-up messages are shown on the glass until then, and a stop says why.

**Speed, measured in PCBKit's simulator** (instruction-accurate CH32V003 at 48 MHz, the memories'
SPI timing): Doom's start-up messages from the first second, its title screen at about 200 s of
device time, the demo at 300 s, then a frame every several seconds. It is a demonstration
board, not a game console — and the reason it is variant 01.

## Power

| State | Current at 3.3 V (estimate) |
|---|---|
| CH32V003 at 48 MHz | ~5 mA |
| PSRAM active (SPI, low clock) | ~3–5 mA (datasheet: 5.5 mA typ at 133 MHz) |
| W25Q128 (only while booting) | ~10 mA |
| PCD8544 + backlight through 330 Ω | ~0.3 mA + ~4 mA |
| **Total** | **~15–20 mA ≈ 60 mW** |

Two 18650 Li-ion cells in Keystone 1042 holders on the back, in parallel (1S2P, 3.0–4.2 V). From
the cells to the chips:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). It cannot help one cell reversed against the other: in parallel
  they would short each other, so check both against the silkscreen's + before closing a case.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack. Flat-top unprotected 18650 cells fit
  the holders.
- **USB-C input:** a power-only receptacle (5.1 kΩ on CC1 and CC2: any USB-C charger or computer
  gives 5 V), a 750 mA resettable fuse (nSMD075) and a 5 V TVS diode (SMF5.0A) before the charger.
- **Charger with power path:** a TI BQ24075 in USB500 mode (at most 500 mA from the port), ISET
  for 800 mA (RISET 1.13 kΩ), RILIM 1.18 kΩ, safety timers off (two cells in parallel charge longer
  than its longest timer; termination is by current), TS tied to 10 kΩ (no thermistor). A linear
  charger passes on at most what it takes in, so from USB the cells charge at up to about 500 mA.
  Its output SYS runs the board from USB when one is plugged in and from the cells otherwise.
- **3.3 V:** an AP2112K-3.3 LDO from SYS (the old TPS61023 boost cannot make 3.3 V from a Li-ion
  cell above it). At this board's 20 mA its dropout is a few mV, so 3.3 V holds down to about
  3.3 V on the cells and then follows them; every chip here runs from 2.7 V.
- **On/off:** the slide switch at the top edge drives the LDO's enable (1 MΩ pulls it off); the
  charger charges with the switch off. Off, the board draws only the charger's and the protection's
  few µA.

Two 3000 mAh cells (about 22 Wh) at 60 mW: **about 300 h** of (very slow) play (estimate). A
full charge of two empty cells from USB at about 500 mA takes about 12 h.

## Charging

Plug any USB-C charger or computer into J1 at the left edge. The red LED (CHG) is on while the
cells charge and goes out when they are full; the green LED (USB) is on while USB power is
present. The board runs from USB while it charges, switched on or off. Insert the cells with +
at the end the back's silkscreen marks.

## Modules plugged in by hand

The assembly solders a 1x8 female socket for the display; the module is bought separately:

- **DS1: Nokia 5110 LCD module** (PCD8544, 84 × 48, the common red board with an 8-pin header:
  RST, CE, DC, DIN, CLK, VCC, BL, GND in that order), its pins soldered on as it comes, plugged in
  on the front. Boards with another pin order exist: check the order on its silkscreen against
  the board's before plugging it in.

## Firmware: build and flash

- **Build:** in PCBKit, the Laboratory's **Build & run** builds both programs (the CH32V003's with
  RV32EC, ilp32e, -Os; the guest for RV32IMAC with the WAD); then export the firmware
  (`export.run { format: "firmware" }`): `U1.bin` for the CH32V003 and `U3.bin` for the W25Q128.
- **The CH32V003, on arrival:** a WCH-LinkE (about $5, USB) on J2: its 3V3, SWIO and GND to J2's
  (marked on the back). Then `minichlink -w U1.bin flash -b` (ch32v003fun) or WCH-LinkUtility.
- **The W25Q128 (the guest):** hold MENU while switching on: the emulator lets go of the SPI bus
  (every pin an input) and waits, so a USB SPI programmer (CH341A) with a SOIC-8 clip on the
  W25Q128 writes it in place: `flashrom -p ch341a_spi -w U3.bin`. Switch off and on: Doom boots.

## Bill of materials

Key parts and a rough cost at 10 pieces (estimates, LCSC/AliExpress prices of 2026):
CH32V003F4P6 $0.30 · APS6404L $1.20 · W25Q128JV $1.00 · Nokia 5110 module $2.50 ·
switch $0.10 · buttons 8 × $0.05 · charger, protection, LDO, USB-C and passives $2.20 · two 18650
holders $9.80 · PCB (2-layer, 96 × 92) $1.50 → **about $20 per unit** without the cells.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 1 | BZ1 | GPC12075YB-5V4000 | C252948 | Buzzer_D12.0mm_H7.5mm_P6.50mm |
| 7 | C1, C2, C3, C5, C7, C8, C9 | 100nF | C1525 | C_0402_1005Metric |
| 3 | C4, C6, C10 | 1uF | C52923 | C_0402_1005Metric |
| 1 | D1 | tvs 5V | C284108 | D_SOD-123 |
| 1 | DS1 | Nokia 5110 84x48 LCD module (PCD8544): connector (8-pin) | C2905417 | PinHeader_1x08_P2.54mm_Vertical |
| 1 | F1 | PTC 680mA | C70078 | Fuse_1206_3216Metric |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P | C165948 | USB_C_Receptacle_USB2.0_16P |
| 1 | J2 | PinHeader_1x03_P2.54mm_Vertical | C49257 | PinHeader_1x03_P2.54mm_Vertical |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 7 | R1, R2, R7, R8, R9, R10, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 1 | R3 | 330ohm | C23138 | R_0603_1608Metric |
| 1 | R4 | 1Mohm | C26083 | R_0402_1005Metric |
| 1 | R5 | 100kohm | C25741 | R_0402_1005Metric |
| 1 | R6 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R11 | 1kohm | C11702 | R_0402_1005Metric |
| 2 | R12, R13 | 5.1kohm | C25905 | R_0402_1005Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | TS-1187A-B-A-B | C318884 | SW_Tactile_SPST_5.1x5.1mm_SMD |
| 1 | SW9 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 1 | U1 | CH32V003F4P6 | C5187096 | TSSOP-20_4.4x6.5mm_P0.65mm |
| 1 | U2 | APS6404L-3SQR-SN | C5333729 | SOIC-8_3.9x4.9mm_P1.27mm |
| 1 | U3 | W25Q128JVSIQ | C97521 | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U4 | AP2112K-3.3TRG1 | C51118 | SOT-23-5 |
| 1 | U5 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U6 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
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
- **Board:** two 18650 cells as a discharge curve behind the charger's power path and an LDO.

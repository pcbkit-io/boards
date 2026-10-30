# 08 · s3-handheld — Xtensa LX7 with 8 MB of PSRAM, the classic ESP32 Doom

The best-trodden path to a Doom handheld: an ESP32-S3-WROOM-1-N16R8 module (16 MB flash and 8 MB
octal PSRAM inside), the 2.8" ILI9341 touch TFT, a microSD slot on SDMMC, ten buttons, an I2S
amplifier, and two 18650 cells with protection, a power-path charger from USB-C and a fuel gauge.
JLCPCB assembles everything but the display module, which plugs into the socket it solders.

| | |
|---|---|
| Processor | ESP32-S3-WROOM-1-N16R8: ESP32-S3, 2 × **Xtensa LX7** (windowed registers, 128-bit SIMD "PIE" extension, single-precision FPU) at 240 MHz, 512 KB SRAM, 16 MB quad flash, 8 MB **octal** PSRAM (OPI, 80 MHz DDR, behind the cache and MMU) |
| Display | 2.8" 240 × 320 ILI9341 + XPT2046 touch on SPI2 at 40 MHz (IO12 SCK, IO11 MOSI, IO13 MISO, IO10 CS, IO9 DC, IO14 RESET, IO21 backlight; touch CS IO47, IRQ IO48) |
| Storage | microSD on SDMMC host, 1-bit mode (IO39 CLK, IO38 CMD, IO40 D0; D1–D3 pulled up) |
| Input | 10 × 6 mm tact switches: up IO15, down IO16, left IO17, right IO7, fire IO2, use IO41, strafe IO42 / IO1, MENU IO0 (= BOOT), enter RXD0; RESET (EN) beside them |
| Sound | I2S0 (IO4 BCLK, IO5 LRCLK, IO6 DIN) → MAX98357A (mono mix) → 8 Ω speaker on J5 (bottom edge); amplifier on SYS |
| Power | 2 × 18650 Li-ion in parallel (1S2P, a 2 A resettable fuse per cell) in Keystone 1042 holders → reverse-cell P-FET (AO3401A) → DW01A + FS8205A; USB-C → **BQ24075** (power path, USB500, 800 mA charge) → SYS → TLV75733 (1 A; EN on the slide switch) → 3.3 V; **MAX17048** fuel gauge on the pack, I2C (IO8 SDA, IO18 SCL, 0x36) |
| Programming | USB-C to the S3's USB (IO19/IO20): USB Serial/JTAG, `esptool`, or TinyUSB; RESET and BOOT (= MENU) on the front |
| Board | 156 × 110 mm, **4 layers**, four M3 holes; front: display, buttons, RESET, on/off switch, charger and its LEDs, test pads, speaker connector; back: module (top edge, antenna 6 mm past it), USB-C (top edge, beside the module), microSD (bottom edge), amplifier, regulator, fuel gauge, the two cells |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 3 warnings (diff-pair-uncoupled, erc-rail-name), 0 unrouted nets. 77 components, 62 nets, 665 track segments (3041 mm), 79 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partly by family estimates. Nothing to fix, 3 to consider, 18 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
<!-- /STATUS -->

Warnings left, and why they are acceptable:

| Warning | Why |
|---|---|
| `diff-pair-uncoupled`, `skew` on DP / DN | USB full speed (12 Mbit/s): the pair runs from the connector beside the module around it to the module's USB pins, partly apart; tens of mm uncoupled and of skew are tenths of a nanosecond against 4–20 ns edges |
| heat sources need exact thermal parameters; chips estimated by their family | the thermal estimate's own caveat: nothing on this board dissipates more than a few hundred mW |

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/doom/s3-handheld, open the
Laboratory and press **Build & run**: the page builds `doom/` for the ESP32-S3's Xtensa LX7 with
Espressif's LLVM (with `DOOM1.WAD` linked into its flash), boots it the way the chip's boot ROM
boots an image from flash, and draws the ILI9341's frames on the board. Play with the keyboard
(arrows, F fire, Space use, comma/period strafe, M menu, Enter) or a gamepad (`input.js`).

## Why it runs Doom

The program runs in place from the module's flash through the instruction cache, and the game's
data, the shareware WAD, is linked into the same flash and read in place through the data cache:
no SD card or partition table is needed. The start-up (PCBKit's ESP32-S3 kit) stops the boot ROM's
watchdogs, moves the CPU to the PLL at 160 MHz, maps the program's flash pages and brings the octal
PSRAM up as ESP-IDF does (20 MHz, no timing tuning), which holds the zone heap (5 MB), the four
screens and the visplanes; internal SRAM keeps the engine's other state and the stack. The board's
copy of the engine keeps its trigonometric tables in flash and puts two large arrays in the PSRAM
(`doom/doom_esp32s3.c` says where). Each frame goes through Doom's palette to RGB565 and to the
ILI9341's middle 320 × 200 (landscape) on GP-SPI2 at 40 MHz, 64 bytes a transaction: about 26 ms of
the bus a frame, so the display, not the CPU, caps it near 25 fps. Measured in PCBKit's simulator:
the title by 4 s of device time, the demo by 9 s; the simulator runs it faster than real time.

## Power

Two 18650 Li-ion cells in Keystone 1042 holders on the back, in parallel (1S2P, 3.0–4.2 V). From the
cells to the board:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). One cell reversed against the other is what the **per-cell fuses**
  are for: each holder's + reaches the pack through its own 2 A resettable fuse (Littelfuse
  1812L200/16GR: 16 V, 100 A interrupting), so a cell put in backwards, or one much fuller than
  the other, trips both in milliseconds instead of shorting the two cells together (tens of
  amperes); take the cell out and they reset. Still put each cell's + at the end the silkscreen
  marks.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack.
- **Charger with power path:** a TI BQ24075 (the BQ24074's sibling with SYSOFF and a fixed
  termination, as on doom/rp2350-handheld) from the USB-C's VBUS (5.1 kΩ CC pull-downs), in
  USB500 mode (at most 500 mA from the port), 800 mA fast charge (RISET 1.13 kΩ), input limit by
  RILIM 1.18 kΩ, safety timers off (two cells charge longer than its longest timer; termination is
  by current), TS tied to 10 kΩ (no thermistor). Its output SYS runs the board from USB when one is
  plugged in and from the cells otherwise.
- **Rails:** SYS feeds the TLV75733 LDO (3.3 V, 1 A, room for the module's 500 mA radio peaks: the module, the display, the card) and the
  MAX98357A directly. Near empty (3.0–3.3 V cells) the LDO drops out and 3.3 V follows the cells;
  the S3 runs to 3.0 V, and the fuel gauge tells the firmware when to stop.
- **Fuel gauge:** the MAX17048 measures the pack after the reverse-cell FET (ModelGauge, no sense
  resistor) and reports its state of charge on I2C.
- **On/off:** the slide switch on the left edge drives the LDO's enable (off: the 3.3 V rail is off,
  the amplifier shut down by its SD_MODE pull-up; the charger still charges).

| State | Current (estimate) |
|---|---|
| ESP32-S3, two cores at 240 MHz, radio off | 60–90 mA |
| PSRAM active | ~15 mA |
| ILI9341 with backlight | 60–80 mA |
| MAX98357A | 20–200 mA from SYS |
| **Typical** | **~200 mA at 3.7 V** |

Two 3000 mAh cells → **~25 h**; with Wi-Fi on (peaks 350 mA) less. A full charge of two empty cells
at 800 mA takes about 8 h.

## Charging

Plug any USB-C charger or computer into the USB-C on the back's top edge. The red LED (CHG) is on
while the cells charge and goes out when they are full; the green LED (USB) is on while USB power
is present. The board runs from USB while it charges, switched on or off. Insert the cells with +
at the end the back's silkscreen marks.

## Modules plugged in by hand

The assembly solders a 1x14 female socket; the module is bought separately and plugged in:

- **DS1: 2.8" 240 × 320 ILI9341 SPI TFT with XPT2046 touch** (MSP2807, 14 pins: VCC, GND, CS,
  RESET, DC, SDI, SCK, LED, SDO, T_CLK, T_CS, T_DIN, T_DO, T_IRQ), on the front.
- An 8 Ω speaker (1–3 W) on the JST PH 2-pin connector J5 at the bottom edge; a microSD card in J4.

The ESP32-S3-WROOM-1-N16R8 itself is soldered by the assembly (LCSC C2913202).

## Firmware: build and flash

- **Build:** in PCBKit, the Laboratory's **Build & run** builds `doom/` (Xtensa LX7, Espressif's
  LLVM, the kit's start-up and link layout); then export the firmware
  (`export.run { format: "firmware" }`): `U1.bin`, the whole flash from offset 0 (the boot image the
  ROM loads, then the program's code and constants and the WAD at their pages). No 2nd-stage
  bootloader or partition table: the ROM boots the image directly.
- **Flashing on arrival:** switch the board on and plug its USB-C into a computer: the S3's
  USB Serial/JTAG enumerates (303a:1001, `/dev/ttyACM0` or a COM port) even with the module's flash
  blank, and `esptool` resets the chip into its ROM download mode through it by itself:
  `esptool.py --chip esp32s3 -p /dev/ttyACM0 write_flash 0x0 U1.bin` (or the browser's ESP Web
  Flasher at offset 0). If a running program has taken the USB pins over: hold MENU (IO0), press and
  release RESET, release MENU: ROM download mode, then flash as above. Press RESET: Doom starts.

## Bill of materials

Rough cost at 10 pieces (estimates, JLCPCB prices): ESP32-S3-WROOM-1-N16R8 $4.50 · ILI9341 2.8" touch
$7.00 · microSD socket $0.90 · MAX98357A $1.00 · BQ24075 $1.10 · MAX17048 $1.20 · DW01A + FS8205A +
AO3401A $0.25 · TLV75733 $0.15 · two Keystone 1042 $9.80 · USB-C + ESD $0.40 · buttons $3.00 ·
passives $1.00 · 4-layer PCB $4.00 · speaker $1.00 → **about $35** without the cells and assembly fees.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 1 | C1 | 10uF | C19702 | C_0603_1608Metric |
| 1 | C2 | 22uF | C29277 | C_0805_2012Metric |
| 1 | C3 | 22uF | C5672 | C_1206_3216Metric |
| 4 | C4, C7, C8, C11 | 100nF | C1525 | C_0402_1005Metric |
| 1 | C5 | 1uF | C52923 | C_0402_1005Metric |
| 1 | C6 | 10uF | C15850 | C_0805_2012Metric |
| 3 | C12, C13, cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | D1 | tvs 5V | C502527 | D_SOD-323 |
| 1 | DS1 | MSP2807 2.8in 240x320 SPI TFT module with touch (ILI9341) | C2905420 | TFT_2.8in_ILI9341_Socket_1x14_P2.54mm |
| 2 | F1, F2 | PTC 2000mA | C18198340 | Fuse_1812_4532Metric |
| 1 | J1 | USB4105-GF-A | C165948 | USB_C_Receptacle_USB2.0_16P |
| 1 | J4 | DM3AT-SF-PEJM5 | C114218 | Hirose_DM3AT_microSD_socket_push-push |
| 1 | J5 | S2B-PH-K-S(LF)(SN) | C173752 | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 9 | R1, R2, R3, R4, R5, R6, R7, R9, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 2 | R8, R13 | 100kohm | C25741 | R_0402_1005Metric |
| 1 | R10 | 634kohm | C270588 | R_0402_1005Metric |
| 2 | R11, R12 | 5.1kohm | C25905 | R_0402_1005Metric |
| 1 | R14 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R15 | 1kohm | C11702 | R_0402_1005Metric |
| 2 | R18, R19 | 3.3kohm | C25890 | R_0402_1005Metric |
| 10 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10 | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW11 | TS-1187A-B-A-B | C318884 | SW_Tactile_SPST_5.1x5.1mm_SMD |
| 1 | SW12 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 1 | U1 | ESP32-S3-WROOM-1-N16R8 | C2913202 | ESP32-S3-WROOM-1_module_18x25.5mm_40_edge_pads_EPAD |
| 1 | U2 | USBLC6-2SC6 | C7519 | SOT-23-6 |
| 1 | U4 | TLV75733PDBVR | C485517 | SOT-23-5 |
| 1 | U5 | MAX98357AETE+T | C910544 | QFN-16-1EP_3x3mm_P0.5mm_EP1.1x1.1mm |
| 1 | U6 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U7 | MAX17048G+T10 | C2682616 | DFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm |
| 1 | U8 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** **Xtensa LX7** as Espressif configures it: windowed register file (64 physical registers,
  `ENTRY`/`RETW`, window overflow/underflow exceptions), zero-overhead loops, `MUL16/32`, `DIV32`,
  the single-precision FPU, the PIE SIMD instructions (used by esp-dsp, not by Doom), and the
  exception/interrupt levels (vectors per level, `EXCSAVE`, `INTENABLE`, `CCOUNT`/`CCOMPARE` timers).
  The configuration (which options, which vectors) comes from Espressif's overlay — a core described
  partly as data.
- **Interrupts:** the **interrupt matrix** (peripheral sources → 32 CPU interrupts per core).
- **Boot ROM:** strapping pins (IO0, IO3, IO45, IO46), the ROM loader reading the 2nd-stage
  bootloader's image from flash, the download protocol over USB Serial/JTAG/UART.
- **Memory:** the **MSPI with cache and MMU** in front of quad flash and **octal PSRAM** (the
  heap lives there: PSRAM must be a writable mapped memory with cache timing).
- **Clocks:** PLL → 240 MHz CPU, 80 MHz APB; SYSTIMER for FreeRTOS on both cores; two cores (SMP
  FreeRTOS: the second core's start, cross-core interrupts, spinlocks via `S32C1I`).
- **Peripherals:** GPSPI2 + **GDMA**, SDMMC host (with its internal DMA descriptors), I2S0 + GDMA,
  I2C0, the GPIO matrix / IO MUX (the pin-mux table), USB Serial/JTAG, TIMG watchdogs, eFuse, RTC
  controller registers the IDF touches at start.
- **Board:** ILI9341 + XPT2046, SD card (SD protocol, not only SPI mode), MAX98357A, two 18650 cells behind DW01A protection and
  the BQ24075 power path (input current limit, charge phases, SYS), MAX17048 as an I2C model reading the battery model.

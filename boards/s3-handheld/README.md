# 08 · s3-handheld — Xtensa LX7 with 8 MB of PSRAM, the classic ESP32 Doom

The best-trodden path to a Doom handheld: an ESP32-S3-WROOM-1-N16R8 module (16 MB flash and 8 MB
octal PSRAM inside), the 2.8" ILI9341 touch TFT, a microSD slot on SDMMC, ten buttons, an I2S
amplifier, and a complete LiPo system with power path and fuel gauge.

| | |
|---|---|
| Processor | ESP32-S3-WROOM-1-N16R8: ESP32-S3, 2 × **Xtensa LX7** (windowed registers, 128-bit SIMD "PIE" extension, single-precision FPU) at 240 MHz, 512 KB SRAM, 16 MB quad flash, 8 MB **octal** PSRAM (OPI, 80 MHz DDR, behind the cache and MMU) |
| Display | 2.8" 240 × 320 ILI9341 + XPT2046 touch on SPI2 at 40 MHz (IO12 SCK, IO11 MOSI, IO13 MISO, IO10 CS, IO9 DC, IO14 RESET, IO21 backlight; touch CS IO47, IRQ IO48) |
| Storage | microSD on SDMMC host, 1-bit mode (IO39 CLK, IO38 CMD, IO40 D0; D1–D3 pulled up) |
| Input | 10 × 6 mm tact switches: IO15, IO16, IO17, IO7, IO2, IO41, IO42, IO1, IO0 (MENU = BOOT), RXD0 |
| Sound | I2S0 (IO4 BCLK, IO5 LRCLK, IO6 DIN) → MAX98357A (mono mix) → 8 Ω speaker; amplifier on VSYS |
| Power | USB-C → **BQ24074** (power path: VSYS 4.4 V from USB while charging, the cell behind it; ILIM ≈ 1.4 A, ISET 1.13 kΩ ≈ 800 mA charge) → TLV75533 (EN on the slide switch) → 3.3 V; LiPo on JST PH; **MAX17048** fuel gauge on I2C (IO8 SDA, IO18 SCL, 0x36) |
| Programming | USB-C to the S3's USB (IO19/IO20): USB Serial/JTAG, `esptool`, or TinyUSB; RESET on the back, BOOT = MENU on the front |
| Board | 156 × 66 mm, **4 layers**; front: display, buttons, switch, USB-C; back: module (top edge, antenna 6 mm past it), SD, power, amplifier |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 3 warnings, 0 unrouted nets; 62 components, 52 nets, 531 track segments (2219 mm), 69 vias.

| Warning | Why it is acceptable |
|---|---|
| `track-width-class`: DP: 1 piece of track (1.1 mm in all) down to 0.19 mm, narrower than its class's 0.25 mm: the router narrowed it where the parts left no room. Fine for a signal; | a 1–4 mm neck at a fine-pitch pad (signal, or a supply under ~0.5 A: short necks are within IPC-2221 limits) |
| `diff-pair-uncoupled`: pair DP / DN: 25.82 mm uncoupled (36.85 mm coupled), over the pair's default's 10 mm | USB full speed (12 Mbit/s): the connector is on the front edge, the processor on the back; tens of mm uncoupled are harmless at this rate |
| `skew`: DN: 64.26 mm, 2.04 mm shorter than DP (66.31 mm), over the pair's default's 0.1 mm skew | USB full speed: millimetres of skew are picoseconds against 4–20 ns edges |

<!-- /STATUS -->

## Why it runs Doom

This is how most ESP32 Doom ports run (Espressif's own `esp32-doom` demo on the WROVER kit used the
same idea on the original ESP32 with SPI PSRAM): PrBoom with its zone heap and caches in PSRAM, the
WAD memory-mapped from a flash partition (or loaded from the SD card), the 320 × 200 8-bit frame in
internal SRAM, a second core converting it to RGB565 and pushing it to the ILI9341 by SPI DMA
(153 KB at 40 MHz: ~31 ms — the display, not the CPU, caps it near 30 fps; estimate).

## Power

| State | Current (estimate) |
|---|---|
| ESP32-S3, two cores at 240 MHz, radio off | 60–90 mA |
| PSRAM active | ~15 mA |
| ILI9341 with backlight | 60–80 mA |
| MAX98357A | 20–200 mA from VSYS |
| **Typical** | **~200 mA at 3.7 V** |

A 2000 mAh LiPo → **~10 h**; with Wi-Fi on (peaks 350 mA) less. The power path lets it play from USB
with an empty or missing cell; the MAX17048 reports state of charge (ModelGauge, no sense resistor).

## Firmware: build and flash

- ESP-IDF 5.x, `idf.py set-target esp32s3`, toolchain `xtensa-esp32s3-elf-gcc`; `CONFIG_SPIRAM_MODE_OCT`,
  heap in PSRAM (`CONFIG_SPIRAM_USE_MALLOC`); the WAD in a `data` partition (`esp_partition_mmap`)
  or on the SD card (FatFs over SDMMC).
- Image format: Espressif's app image (segments with load addresses, SHA-256) after the 2nd-stage
  bootloader and the partition table.
- Flash: `idf.py -p /dev/ttyACM0 flash monitor`. Recovery: hold MENU (IO0), tap RESET → ROM download mode.

## Bill of materials

Rough cost at 10 pieces (estimates): ESP32-S3-WROOM-1-N16R8 $4.50 · ILI9341 2.8" touch $7.00 ·
microSD socket $0.60 · MAX98357A $0.90 · BQ24074 $1.80 · MAX17048 $1.60 · TLV75533 $0.20 · USB-C +
ESD $0.40 · buttons $0.50 · passives $1.50 · 4-layer PCB $4.00 · speaker $1.00 · 2000 mAh LiPo $6.00
→ **about $30**.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 1 | C1 | 10uF |  | C_0603_1608Metric |
| 1 | C2 | 22uF |  | C_0805_2012Metric |
| 1 | C3 | 22uF |  | C_1206_3216Metric |
| 3 | C4, C7, C11 | 100nF |  | C_0402_1005Metric |
| 2 | C5, C8 | 1uF |  | C_0402_1005Metric |
| 1 | C6 | 10uF |  | C_0805_2012Metric |
| 2 | C9, C10 | 4.7uF |  | C_0805_2012Metric |
| 2 | C12, C13 | 1uF |  | C_0603_1608Metric |
| 1 | D1 | tvs 5V |  | D_SOD-323 |
| 1 | D2 | red |  | LED_0603_1608Metric |
| 1 | D3 | green |  | LED_0603_1608Metric |
| 1 | DS1 |  | MSP2807 2.8in 240x320 SPI TFT module with touch (ILI9341) | TFT_2.8in_ILI9341_Socket_1x14_P2.54mm |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P |  | USB_C_Receptacle_USB2.0_16P |
| 2 | J2, J5 |  | S2B-PH-K-S(LF)(SN) | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | J4 |  | DM3AT-SF-PEJM5 | Hirose_DM3AT_microSD_socket_push-push |
| 9 | R1, R2, R3, R4, R5, R6, R7, R9, R17 | 10kohm |  | R_0402_1005Metric |
| 1 | R8 | 100kohm |  | R_0402_1005Metric |
| 1 | R10 | 634kohm |  | R_0402_1005Metric |
| 2 | R11, R12 | 5.1kohm |  | R_0402_1005Metric |
| 1 | R13 | 1.13kohm |  | R_0402_1005Metric |
| 1 | R14 | 1.18kohm |  | R_0402_1005Metric |
| 1 | R15 | 4.12kohm |  | R_0402_1005Metric |
| 1 | R16 | 46.4kohm |  | R_0402_1005Metric |
| 2 | R18, R19 | 3.3kohm |  | R_0402_1005Metric |
| 11 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10, SW11 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW12 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | U1 |  | ESP32-S3-WROOM-1-N16R8 | ESP32-S3-WROOM-1_module_18x25.5mm_40_edge_pads_EPAD |
| 1 | U2 | ESD 2-line |  | SOT-23-6 |
| 1 | U4 |  | TLV75533PDBVR | SOT-23-5 |
| 1 | U5 |  | MAX98357AETE+T | QFN-16-1EP_3x3mm_P0.5mm_EP1.1x1.1mm |
| 1 | U6 |  | BQ24074RGTR | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U7 |  | MAX17048G+T10 | DFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm |
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
- **Board:** ILI9341 + XPT2046, SD card (SD protocol, not only SPI mode), MAX98357A, BQ24074 power
  path (input current limit, charge phases, VSYS), MAX17048 as an I2C model reading the battery model.

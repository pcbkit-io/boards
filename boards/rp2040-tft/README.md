# 04 · rp2040-tft — the bare RP2040 with a colour screen and an SD card

Variant 02's processor without the module: the RP2040 chip with Raspberry Pi's minimal design
(12 MHz crystal, 16 MB W25Q128 QSPI flash, BOOTSEL), a 1.8" colour TFT, a microSD slot for the WAD,
PWM sound through a class-D amplifier, and a protected one-cell LiPo with a TP4056 charger.

| | |
|---|---|
| Processor | RP2040 (QFN-56): 2 × **Cortex-M0+** (ARMv6-M), 133 MHz rated (rp2040-doom: 270 MHz), 264 KB SRAM, no internal flash; W25Q128JVSIQ 16 MB on the QSPI (XIP), 12 MHz crystal |
| Display | 1.8" 128 × 160 TFT, ST7735S (MSP1803), SPI0 (GPIO18 SCK, GPIO19 MOSI, GPIO17 CS, GPIO20 A0/DC, GPIO21 RESET), backlight GPIO22 |
| Storage | microSD (Hirose DM3AT) on SPI1 (GPIO10–13), card detect GPIO14 |
| Input | 8 × 6 mm tact switches: GPIO2–GPIO9 |
| Sound | GPIO15 PWM → 1 kΩ / 10 nF low-pass → PAM8403 (left channel, shutdown on GPIO16) → 8 Ω speaker on JST PH |
| Power | LiPo (JST PH) → DW01A + FS8205A → VBAT; TP4056 charger (1.2 kΩ PROG: 1 A) from USB-C VBUS; TLV75533 LDO 500 mA, enabled by the slide switch; the PAM8403 runs from VBAT |
| Programming | USB-C to the RP2040's USB (27 Ω series, ESD array): BOOTSEL button → UF2; SWD on a 3-pin header |
| Board | 132 × 58 mm, **4 layers** (GND plane on In1); front: display, buttons, switch, USB-C; back: everything else |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 4 warnings, 0 unrouted nets; 79 components, 63 nets, 856 track segments (1819 mm), 101 vias.

| Warning | Why it is acceptable |
|---|---|
| `track-width-class`: CONN_DN: 1 piece of track (1.47 mm in all) down to 0.19 mm, narrower than its class's 0.25 mm: the router narrowed it where the parts left no room. Fine for a s | a 1–4 mm neck at a fine-pitch pad (signal, or a supply under ~0.5 A: short necks are within IPC-2221 limits) |
| `track-width-class`: CONN_DP: 1 piece of track (4.12 mm in all) down to 0.19 mm, narrower than its class's 0.25 mm: the router narrowed it where the parts left no room. Fine for a s | a 1–4 mm neck at a fine-pitch pad (signal, or a supply under ~0.5 A: short necks are within IPC-2221 limits) |
| `diff-pair-uncoupled`: pair CONN_DP / CONN_DN: 40.19 mm uncoupled (0 mm coupled), over the pair's default's 10 mm | USB full speed (12 Mbit/s): the connector is on the front edge, the processor on the back; tens of mm uncoupled are harmless at this rate |
| `skew`: CONN_DN: 32.78 mm, 5.22 mm shorter than CONN_DP (38 mm), over the pair's default's 0.1 mm skew | USB full speed: millimetres of skew are picoseconds against 4–20 ns edges |

<!-- /STATUS -->

## Why it runs Doom

Same software as 02 (rp2040-doom), with three differences: 16 MB of flash (the full shareware WAD,
or Doom II in WHD form, fits beside the code), the WAD can also come from the SD card (FatFs over
SPI1, read into flash on first boot), and the video back end writes 160 × 100 (half resolution) or
a cropped 128 × 160 portrait of the 320 × 200 frame to the ST7735 as RGB565 by DMA. 128 × 160 × 2 B =
40 KB per frame at 31 MHz SPI: ~10 ms. Sound: Doom's mixer at 11 kHz into a PWM slice at ~120 kHz.

## Power

| State | Current (estimate) |
|---|---|
| RP2040 at 270 MHz, both cores | ~50 mA |
| ST7735 with backlight | ~25 mA |
| SD card reading (bursts) | 20–60 mA |
| PAM8403 at moderate volume | 30–100 mA from VBAT |
| **Typical** | **~100 mA at 3.7 V** |

A 1200 mAh pouch cell → **~10 h**. TP4056 at 1 A charges it in ~1.5 h. The TLV75533 drops at most
238 mV at 500 mA: it regulates down to ~3.5 V on the cell.

## Firmware: build and flash

- Pico SDK 2.x (`PICO_BOARD` a custom header: W25Q128, 12 MHz, `PICO_FLASH_SIZE_BYTES 16M`),
  `arm-none-eabi-gcc`; rp2040-doom with an ST7735 + FatFs back end.
- Flash: hold BOOTSEL (pulls QSPI_SS low through 1 kΩ), plug USB-C, copy the UF2. Or `openocd -f
  interface/cmsis-dap.cfg -f target/rp2040.cfg -c "program doom.elf verify reset exit"` over J3.

## Bill of materials

Rough cost at 10 pieces (estimates): RP2040 $0.80 · W25Q128 $1.00 · crystal $0.15 · ST7735 module
$3.00 · microSD socket $0.60 · PAM8403 $0.15 · TP4056 $0.10 · DW01A + FS8205A $0.15 · TLV75533 $0.20
· USB-C + ESD + TVS $0.40 · buttons $0.40 · passives $1.00 · 4-layer PCB $3.00 · speaker $1.00 · 1200
mAh LiPo $4.50 → **about $17**, plus assembly of a QFN-56.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 12 | C1, C2, C3, C4, C5, C9, C12, C13, C14, C15, C18, C21 | 100nF |  | C_0402_1005Metric |
| 1 | C6 | 10uF |  | C_0603_1608Metric |
| 1 | C7 | 10nF |  | C_0402_1005Metric |
| 3 | C8, C22, C23 | 1uF |  | C_0603_1608Metric |
| 6 | C10, C11, C24, C25, C26, C28 | 1uF |  | C_0402_1005Metric |
| 2 | C16, C17 | 15pF |  | C_0402_1005Metric |
| 2 | C19, C20 | 10uF |  | C_0805_2012Metric |
| 1 | C27 | 22uF |  | C_1206_3216Metric |
| 1 | C29 | 470nF |  | C_0402_1005Metric |
| 1 | D1 | tvs 5V |  | D_SOD-323 |
| 1 | D2 | red |  | LED_0603_1608Metric |
| 1 | D3 | green |  | LED_0603_1608Metric |
| 1 | DS1 |  | MSP1803 1.8in 128x160 SPI TFT module (ST7735S) | TFT_1.8in_ST7735_Socket_1x08_P2.54mm |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P |  | USB_C_Receptacle_USB2.0_16P |
| 2 | J2, J5 |  | S2B-PH-K-S(LF)(SN) | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | J3 | PinHeader_1x03_P2.54mm_Vertical |  | PinHeader_1x03_P2.54mm_Vertical |
| 1 | J4 |  | DM3AT-SF-PEJM5 | Hirose_DM3AT_microSD_socket_push-push |
| 1 | Q1 |  | FS8205A | TSSOP-8_4.4x2.93mm_P0.65mm |
| 5 | R1, R5, R10, R16, R18 | 1kohm |  | R_0402_1005Metric |
| 2 | R2, R3 | 27ohm |  | R_0402_1005Metric |
| 5 | R4, R6, R7, R8, R9 | 10kohm |  | R_0402_1005Metric |
| 1 | R11 | 100kohm |  | R_0402_1005Metric |
| 2 | R12, R13 | 5.1kohm |  | R_0402_1005Metric |
| 1 | R14 | 1ohm |  | R_0603_1608Metric |
| 1 | R15 | 1.2kohm |  | R_0402_1005Metric |
| 1 | R17 | 100ohm |  | R_0402_1005Metric |
| 9 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW10 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW9 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | U1 |  | RP2040 | QFN-56-1EP_7x7mm_P0.4mm_EP3.1x3.1mm |
| 1 | U2 |  | TP4056 | SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.413x3.302mm |
| 1 | U3 |  | DW01A | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U4 |  | TLV75533PDBVR | SOT-23-5 |
| 1 | U5 |  | PAM8403DR | SOIC-16_3.9x9.9mm_P1.27mm |
| 1 | U6 |  | W25Q128JVSIQ | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U7 | ESD 2-line |  | SOT-23-6 |
| 1 | Y1 | 12MHz 10pF |  | Crystal_SMD_3225-4Pin |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

Everything of variant 02, plus: the boot ROM's **USB mass-storage UF2 loader** if flashing is to be
simulated; the **SSI/XIP with 16 MB** (the current SoC maps 2 MB); **two PL022 SPI** blocks with
**DMA** (paced by the SPI's DREQ); the **PWM slice** as an audio source (duty → sample); an SD card
model in SPI mode (CMD0/8/55/41/58/17/18/24, a FAT image as its contents); an ST7735 model; the
TP4056/DW01A/LDO power chain as a battery model with protection thresholds and charge current.

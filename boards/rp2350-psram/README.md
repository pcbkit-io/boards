# 07 · rp2350-psram — the bare RP2350A with QSPI flash and PSRAM on one bus

The reference device's chip without the module: an RP2350A with Raspberry Pi's minimal design
(core switching regulator, 12 MHz crystal), a 16 MB W25Q128 on QSPI chip select 0 and an 8 MB
APS6404L PSRAM on the QMI's chip select 1 (GPIO8) sharing the same SCLK and SD0–SD3, so both are
memory-mapped through the XIP cache and Doom runs as an ordinary program. Plus the power system the
module boards lack: a BQ24074 charger with power path, a MAX17048 fuel gauge and a switched LDO.

| | |
|---|---|
| Processor | RP2350A (QFN-60): 2 × **Cortex-M33** (ARMv8-M Mainline, FPU, DSP, TrustZone) **or** 2 × **Hazard3** RISC-V (RV32IMAC + Zba/Zbb/Zbs/Zbkb/Zcb/Zcmp), the architecture chosen by the image; 150 MHz; 520 KB SRAM; on-chip 1.1 V switching regulator (3.3 µH) |
| Memory | W25Q128JVSIQ 16 MB on QSPI_SS (CS0); APS6404L-3SQR 8 MB PSRAM on GPIO8 = QMI CS1 (XIP_CS1n); both mapped by the QMI (flash at 0x1000_0000, PSRAM at 0x1100_0000), 16 KB XIP cache |
| Display | 2.0" 240 × 320 IPS, ST7789 (MSP2008), SPI0 (GPIO18 SCK, GPIO19 MOSI, GPIO17 CS, GPIO16 DC, GPIO20 RES, GPIO21 backlight); landscape 320 × 240 |
| Storage | microSD on SPI1 (GPIO10–13), card detect GPIO14 |
| Input | 10 × 6 mm tact switches: GPIO0–5 (d-pad, fire, use), GPIO9, GPIO15 (strafe), GPIO25 (menu), GPIO26 (enter) |
| Sound | PIO I2S (GPIO22 BCK, GPIO23 LRCK, GPIO24 DIN) → MAX98357A → 8 Ω speaker |
| Power | USB-C → **BQ24074** (power path: VSYS 4.4 V from USB, the cell behind it; input limit ≈ 1.4 A, charge ≈ 800 mA) → TLV75533 (EN on the slide switch) → 3.3 V; LiPo on JST PH; **MAX17048** gauge on I2C1 (GPIO6 SDA, GPIO7 SCL, 0x36, alert GPIO29); amplifier on VSYS |
| Programming | USB-C to the RP2350's USB (27 Ω, ESD array): BOOTSEL (1 kΩ on QSPI_SS) → UF2 for ARM or RISC-V; SWD + RUN on a 4-pin header |
| Board | 144 × 64 mm, **4 layers** (In1 GND, also used for short signal hops); front: display, buttons, switch, USB-C; back: everything else. Net classes: 0.15 mm tracks and gaps with 0.45 / 0.2 mm vias by default (the QFN-60's 0.4 mm pitch), 0.3 mm for the supplies |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 5 warnings, 0 unrouted nets; 86 components, 67 nets, 885 track segments (2359 mm), 125 vias.

| Warning | Why it is acceptable |
|---|---|
| `track-width-class`: DN: 1 piece of track (1.1 mm in all) down to 0.15 mm, narrower than its class's 0.25 mm: the router narrowed it where the parts left no room. Fine for a signal; | a 1–4 mm neck at a fine-pitch pad (signal, or a supply under ~0.5 A: short necks are within IPC-2221 limits) |
| `track-width-class`: V3: 3 pieces of track (4.05 mm in all) down to 0.15 mm, narrower than its class's 0.3 mm: the router narrowed it where the parts left no room. Fine for a signal | a 1–4 mm neck at a fine-pitch pad (signal, or a supply under ~0.5 A: short necks are within IPC-2221 limits) |
| `track-width-class`: VSYS: 1 piece of track (1.21 mm in all) down to 0.15 mm, narrower than its class's 0.3 mm: the router narrowed it where the parts left no room. Fine for a signa | a 1–4 mm neck at a fine-pitch pad (signal, or a supply under ~0.5 A: short necks are within IPC-2221 limits) |
| `diff-pair-uncoupled`: pair DP / DN: 36.3 mm uncoupled (0 mm coupled), over the pair's default's 10 mm | USB full speed (12 Mbit/s): the connector is on the front edge, the processor on the back; tens of mm uncoupled are harmless at this rate |
| `skew`: DP: 35.44 mm, 2.75 mm shorter than DN (38.19 mm), over the pair's default's 0.1 mm skew | USB full speed: millimetres of skew are picoseconds against 4–20 ns edges |

<!-- /STATUS -->

## Why it runs Doom

This is the reference device's software on its own board: Doom with its zone heap and screens in
the PSRAM, the WAD in flash (16 MB: the shareware or registered WAD linked as data), on one Hazard3
core at 150 MHz; the second core drives the display (320 × 240 RGB565 = 153 KB by SPI DMA at 62.5
MHz: ~20 ms). Estimated at 25–35 fps (not measured on hardware; PCBKit's simulated reference runs
its Doom at 35 fps with one instruction per clock and no memory latency).

## Power

| State | Current (estimate) |
|---|---|
| RP2350 at 150 MHz + PSRAM + flash | ~45 mA at 3.3 V |
| ST7789 2.0" with backlight | ~40 mA |
| MAX98357A | 20–200 mA from VSYS |
| **Typical** | **~120 mA at 3.7 V** |

A 2000 mAh LiPo → **~16 h**. With USB plugged the BQ24074 runs the device from USB and charges the
cell with what is left of its input limit.

## Firmware: build and flash

- Pico SDK 2.x with a board header for this layout (W25Q128 16 MB; the PSRAM on QMI M1 with chip
  select GPIO8 is configured by the firmware — the boot ROM only sets up CS0), `-DPICO_PLATFORM=
  rp2350-riscv` (or `rp2350-arm-s`).
- Flash: hold BOOTSEL, plug USB-C, copy the UF2; `picotool load -x`; or SWD (J3: SWCLK, GND, SWDIO,
  RUN) with a Raspberry Pi Debug Probe.

## Bill of materials

Rough cost at 10 pieces (estimates): RP2350A $1.10 · W25Q128 $1.00 · APS6404L $1.20 · crystal $0.15 ·
3.3 µH inductor $0.20 · ST7789 2.0" $4.00 · microSD socket $0.60 · MAX98357A $0.90 · BQ24074 $1.80 ·
MAX17048 $1.60 · TLV75533 $0.20 · USB-C + ESD $0.40 · buttons $0.50 · passives $2.00 · 4-layer PCB
$4.00 · speaker $1.00 · 2000 mAh LiPo $6.00 → **about $27**.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 1 | C1 | 10uF |  | C_0603_1608Metric |
| 3 | C2, C3, C18 | 15pF |  | C_0402_1005Metric |
| 15 | C4, C5, C6, C7, C8, C9, C10, C11, C12, C13, C14, C19, C21, C23, C27 | 100nF |  | C_0402_1005Metric |
| 5 | C15, C16, C17, C25, C26 | 4.7uF |  | C_0805_2012Metric |
| 2 | C20, C24 | 1uF |  | C_0402_1005Metric |
| 1 | C22 | 10uF |  | C_0805_2012Metric |
| 2 | C28, C29 | 1uF |  | C_0603_1608Metric |
| 1 | D1 | tvs 5V |  | D_SOD-323 |
| 1 | D2 | red |  | LED_0603_1608Metric |
| 1 | D3 | green |  | LED_0603_1608Metric |
| 1 | DS1 |  | MSP2008 2.0in 240x320 IPS SPI module (ST7789) | TFT_2.0in_ST7789_Socket_1x08_P2.54mm |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P |  | USB_C_Receptacle_USB2.0_16P |
| 2 | J2, J5 |  | S2B-PH-K-S(LF)(SN) | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | J3 | PinHeader_1x04_P2.54mm_Vertical |  | PinHeader_1x04_P2.54mm_Vertical |
| 1 | J4 |  | DM3AT-SF-PEJM5 | Hirose_DM3AT_microSD_socket_push-push |
| 1 | L1 | 3.3uH |  | L_Power_6x6mm |
| 7 | R1, R2, R3, R4, R5, R12, R20 | 10kohm |  | R_0402_1005Metric |
| 1 | R6 | 100kohm |  | R_0402_1005Metric |
| 2 | R7, R11 | 1kohm |  | R_0402_1005Metric |
| 1 | R8 | 33ohm |  | R_0402_1005Metric |
| 2 | R9, R10 | 27ohm |  | R_0402_1005Metric |
| 1 | R13 | 634kohm |  | R_0402_1005Metric |
| 2 | R14, R15 | 5.1kohm |  | R_0402_1005Metric |
| 1 | R16 | 1.13kohm |  | R_0402_1005Metric |
| 1 | R17 | 1.18kohm |  | R_0402_1005Metric |
| 1 | R18 | 4.12kohm |  | R_0402_1005Metric |
| 1 | R19 | 46.4kohm |  | R_0402_1005Metric |
| 2 | R21, R22 | 3.3kohm |  | R_0402_1005Metric |
| 10 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW11 | SW_Tactile_SPST_4.2x3.2mm_SMD |  | SW_Tactile_SPST_4.2x3.2mm_SMD |
| 1 | SW12 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | U1 |  | RP2350A | QFN-60-1EP_7x7mm_P0.4mm_EP3.4x3.4mm |
| 1 | U2 |  | W25Q128JVSIQ | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U3 |  | APS6404L-3SQR-SN | SOIC-8_3.9x4.9mm_P1.27mm |
| 1 | U4 |  | TLV75533PDBVR | SOT-23-5 |
| 1 | U5 |  | MAX98357AETE+T | QFN-16-1EP_3x3mm_P0.5mm_EP1.1x1.1mm |
| 1 | U6 |  | BQ24074RGTR | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U7 |  | MAX17048G+T10 | DFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm |
| 1 | U8 | ESD 2-line |  | SOT-23-6 |
| 1 | Y1 | 12MHz 10pF |  | Crystal_SMD_3225-4Pin |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

The RP2350 SoC being written now (Hazard3, Xh3irq, TIMER, IO_BANK0, SIO, RESETS), plus: the **QMI
with both chip selects** — CS0 set up by the boot ROM, **CS1 configured by the firmware** (M1 timing
and read/write formats, GPIO8 funcsel XIP_CS1n) — in front of **two board memory components** on one
shared QSPI bus (W25Q128, APS6404L); the XIP cache including writes to PSRAM; the boot ROM's
IMAGE_DEF search and BOOTSEL entry by QSPI_SS low; PL022 SPI0/SPI1 with DMA; PIO for I2S; I2C1
(DesignWare) to the MAX17048 model; the core regulator as a fixed 1.1 V; and the board's BQ24074 /
LiPo / LDO chain.

# 10 · teensy41 — Cortex-M7 at 600 MHz: Doom at full speed, Doom II too

The most capable variant: a Teensy 4.1 (NXP i.MX RT1062) on sockets behind a 3.5" 320 × 480 touch
TFT, ten buttons, CD-quality audio to headphones and a speaker, and a LiPo with power path, fuel gauge
and a 5 V boost. The i.MX RT1062 is a BGA-196 crossover MCU with external flash; the Teensy module is
how hobbyists (and small products) use it without a 6-layer BGA board.

| | |
|---|---|
| Processor | Teensy 4.1: NXP i.MX RT1062, Arm **Cortex-M7** (ARMv7E-M, 6-stage dual-issue, double-precision FPU, 32 KB I + 32 KB D cache) at 600 MHz; 1 MB SRAM (512 KB tightly coupled ITCM/DTCM); 8 MB QSPI flash (FlexSPI); pads for 8 MB PSRAM (FlexSPI2); microSD socket on 4-bit SDIO (uSDHC1) |
| Display | 3.5" 320 × 480 ILI9488 + XPT2046 touch (MSP3520) on LPSPI4 (D13 SCK, D11 MOSI, D12 MISO, D10 CS, D9 DC, D8 RESET, D6 backlight PWM; touch CS D5, IRQ D4) |
| Input | 10 × 6 mm tact switches: D0–D3, D14–D17, D22, D24 |
| Sound | SAI1 I2S (D21 BCLK, D20 LRCLK, D7 TX) → **PCM5102A** → 3.5 mm jack and → **PAM8403** (shutdown on D26) → 8 Ω speaker |
| Power | USB-C (power only) → **BQ24074** power path (VSYS 4.4 V) + LiPo; **MAX17048** fuel gauge (D18 SDA, D19 SCL, alert D25); **TPS61023** boost VSYS → 5 V (switched by its EN) → B5819W → Teensy VIN (VIN and VUSB are joined on the Teensy: the diode keeps the boost and a USB host apart) |
| Programming | the Teensy's micro-USB: **Teensy Loader** / `teensy_loader_cli` talking to its MKL02 bootloader chip (HalfKay); the program button forces it |
| Board | 172 × 68 mm, **4 layers**; front: display, buttons, switch, charging USB-C; back: Teensy (its USB at the top edge), audio, power |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 0 warnings, 0 unrouted nets; 73 components, 58 nets, 491 track segments (2495 mm), 47 vias.
<!-- /STATUS -->

## Why it runs Doom

There is nothing to cut: 1 MB of SRAM holds Doom's zone heap for the shareware and registered games
(Chocolate Doom's default is 16 MB, but Doom runs in 2–4 MB; the 8 MB PSRAM pads take the rest when
fitted), the WAD streams from the SD card over 4-bit SDIO, and 600 MHz Cortex-M7 renders 320 × 200
faster than 35 Hz with time to spare (existing Teensy 4 Doom ports run at full rate; not measured
here). The ILI9488 over SPI takes 18-bit colour (3 bytes a pixel): a 320 × 200 game area is 192 KB,
~26 ms at 60 MHz LPSPI with DMA — the display link, again, sets the frame rate.

## Power

| State | Current (estimate) |
|---|---|
| Teensy 4.1 at 600 MHz | ~100 mA at 5 V VIN (PJRC: about 100 mA) |
| ILI9488 3.5" with backlight | ~80–100 mA at 3.3 V |
| PCM5102A + PAM8403 | 20 + 50–200 mA |
| **Typical** | **≈ 1 W → ~300 mA at 3.7 V after the boost** |

A 3000 mAh LiPo → **~9 h**. USB-C charging and play at the same time through the BQ24074's power path.

## Firmware: build and flash

- Teensyduino (Arduino IDE / `arduino-cli`) or PlatformIO `teensy41`, `arm-none-eabi-gcc
  -mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard`; the image is a `.hex` linked for FlexSPI XIP at
  0x6000_0000 with the i.MX boot data: **FlexSPI configuration block (FCB) + IVT + boot data** at the
  start of flash, which the ROM reads.
- Flash: `teensy_loader_cli --mcu=TEENSY41 -w -v doom.hex` (press the program button if the running
  code does not listen for the reboot request).

## Bill of materials

Rough cost at 10 pieces (estimates): Teensy 4.1 $31.50 · 2 × 24-pin sockets $0.80 · MSP3520 3.5"
touch module $11 · PCM5102A $2.00 · PAM8403 $0.15 · jack $0.60 · BQ24074 $1.80 · MAX17048 $1.60 ·
TPS61023 $0.60 · USB-C $0.40 · buttons $0.50 · passives $1.50 · 4-layer PCB $5.00 · speaker $1.00 ·
3000 mAh LiPo $7.00 · microSD card $4 → **about $70**.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 5 | C1, C3, C5, C9, C21 | 100nF |  | C_0402_1005Metric |
| 4 | C2, C4, C6, C22 | 10uF |  | C_0805_2012Metric |
| 2 | C7, C8 | 2.2uF |  | C_0805_2012Metric |
| 2 | C10, C11 | 2.2nF |  | C_0402_1005Metric |
| 5 | C12, C13, C14, C16, C18 | 1uF |  | C_0402_1005Metric |
| 3 | C15, C23, C24 | 22uF |  | C_1206_3216Metric |
| 1 | C17 | 470nF |  | C_0402_1005Metric |
| 2 | C19, C20 | 4.7uF |  | C_0805_2012Metric |
| 1 | D1 | schottky | B5819W | D_SOD-123 |
| 1 | D2 | tvs 5V |  | D_SOD-323 |
| 1 | D3 | red |  | LED_0603_1608Metric |
| 1 | D4 | green |  | LED_0603_1608Metric |
| 1 | DS1 |  | MSP3520 3.5in 320x480 SPI TFT module with touch (ILI9488) | TFT_3.5in_ILI9488_Socket_1x14_P2.54mm |
| 1 | J1 |  | SJ1-3525N | Jack_3.5mm_SameSky_SJ1-352XN_Horizontal |
| 2 | J2, J5 |  | S2B-PH-K-S(LF)(SN) | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | J3 | USB_C_Receptacle_USB2.0_16P |  | USB_C_Receptacle_USB2.0_16P |
| 1 | L1 | 1uH |  | L_Power_6x6mm |
| 5 | R1, R2, R3, R4, R14 | 10kohm |  | R_0402_1005Metric |
| 1 | R5 | 1Mohm |  | R_0402_1005Metric |
| 2 | R6, R7 | 470ohm |  | R_0402_1005Metric |
| 2 | R8, R9 | 5.1kohm |  | R_0402_1005Metric |
| 1 | R10 | 1.13kohm |  | R_0402_1005Metric |
| 1 | R11 | 1.18kohm |  | R_0402_1005Metric |
| 1 | R12 | 4.12kohm |  | R_0402_1005Metric |
| 1 | R13 | 46.4kohm |  | R_0402_1005Metric |
| 2 | R15, R16 | 3.3kohm |  | R_0402_1005Metric |
| 1 | R17 | 732kohm |  | R_0402_1005Metric |
| 1 | R18 | 100kohm |  | R_0402_1005Metric |
| 10 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW12 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | U1 |  | TEENSY41 | Teensy41_Socket_2x24_P2.54mm_W15.24mm |
| 1 | U2 |  | PCM5102APWR | TSSOP-20_4.4x6.5mm_P0.65mm |
| 1 | U3 |  | PAM8403DR | SOIC-16_3.9x9.9mm_P1.27mm |
| 1 | U4 |  | BQ24074RGTR | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U5 |  | MAX17048G+T10 | DFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm |
| 1 | U6 |  | TPS61023DRLR | SOT-563 |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** **ARMv7E-M with FPv5-D16** (double precision), plus the M7's **caches** (CCR, cache
  maintenance by address: the SDK cleans/invalidates around DMA) and **TCM** (ITCM/DTCM at 0x0 /
  0x2000_0000 with FlexRAM banks configured by `IOMUXC_GPR`); MPU regions the SDK programs.
- **Interrupt controller:** NVIC with 160 lines; SysTick; the GPT/PIT timers Teensyduino uses.
- **Boot ROM:** the i.MX RT boot ROM's **serial NOR boot**: read the FCB at 0x6000_0000 to configure
  FlexSPI, then the IVT and boot data, then jump; the image format is that header, not ELF.
- **Clocks (CCM):** ARM PLL to 600 MHz, the dividers for IPG/PERCLK, LPSPI and SAI root clocks;
  the DCDC voltage step Teensyduino performs before raising the clock.
- **Peripherals:** **IOMUXC** pin mux (every pad has a mux register and a pad-control register: the
  pin-mux table from NXP's SVD), GPIO1–9 (fast GPIO6–9 aliases), LPSPI4 + **eDMA with DMAMUX**,
  **SAI1** (I2S) + eDMA, LPI2C1, **uSDHC1** (SD 4-bit with ADMA2 descriptors), USB OTG (the bootloader
  is a separate chip: flashing can be modelled as writing the image directly).
- **Board:** ILI9488 (18-bit pixel format over SPI) + XPT2046, SD card in SD mode, PCM5102A, BQ24074,
  MAX17048, a boost into VIN behind a diode.

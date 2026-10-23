# 05 · f411-psram — an STM32 "Black Pill" chip with 8 MB of SPI PSRAM

The STM32F411CEU6 (the chip of WeAct's Black Pill) is a fast Cortex-M4F with only 128 KB of SRAM and
no memory-mapped QSPI, so the board adds an 8 MB APS6404L PSRAM and a 16 MB W25Q128 on SPI1 and the
port pages Doom's heap through a software cache. A 2.0" IPS, an I2S amplifier, eight buttons, and one
18650 cell. Firmware goes in through the ROM's USB DFU or SWD.

| | |
|---|---|
| Processor | STM32F411CEU6 (UFQFPN-48): Arm **Cortex-M4F** (ARMv7E-M, Thumb-2, DSP, single-precision FPU), 100 MHz, 128 KB SRAM, 512 KB flash with ART accelerator; 25 MHz HSE crystal |
| Memory | APS6404L-3SQR 8 MB PSRAM (CS PA4) and W25Q128JV 16 MB flash (CS PB2) on SPI1 at 50 MHz (PA5 SCK, PA6 MISO, PA7 MOSI) |
| Display | 2.0" 240 × 320 IPS, ST7789 (MSP2008), SPI2 at 25 MHz (PB13 SCK, PB15 MOSI, PB12 CS, PB14 DC, PB1 RES), backlight PWM on PB10 (TIM2_CH3); used landscape 320 × 240 |
| Input | 8 × 6 mm tact switches: PA1–PA3, PB4, PB6–PB9 |
| Sound | I2S3 (PB3 CK, PA15 WS, PB5 SD) → MAX98357A (mono mix, 9 dB) → 8 Ω speaker |
| Power | one 18650 in a Keystone 1042 holder → DW01A + FS8205A; TP4056 1 A charger from USB-C; TLV75533 LDO switched by its EN; amplifier from the cell |
| Programming | USB-C to PA11/PA12: hold BOOT (BOOT0 → 3.3 V) and reset/plug → ROM DFU (`dfu-util`); SWD header (VCC, SWDIO, SWCLK, NRST, GND) for an ST-LINK |
| Board | 140 × 64 mm, **4 layers**; front: display, buttons, BOOT, switch, USB-C; back: MCU, memories, power, the cell |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 1 warnings, 0 unrouted nets; 68 components, 55 nets, 671 track segments (1859 mm), 85 vias.

| Warning | Why it is acceptable |
|---|---|
| `diff-pair-uncoupled`: pair DP / DN: 33.29 mm uncoupled (0 mm coupled), over the pair's default's 10 mm | USB full speed (12 Mbit/s): the connector is on the front edge, the processor on the back; tens of mm uncoupled are harmless at this rate |

<!-- /STATUS -->

## How Doom fits

128 KB of SRAM holds the code's data, the 320 × 200 8-bit frame (64 KB), the stack and a cache;
everything Doom normally keeps in its zone heap (levels, textures, sprites, ~2–4 MB) lives in the
PSRAM. Without memory-mapped QSPI, the port wraps the zone allocator: `Z_Malloc` hands out PSRAM
"handles", hot lumps (the current flats, colormaps, the visplane arrays) are pinned in SRAM, and
everything else is read through a direct-mapped software cache of 256-byte lines; at 50 MHz SPI a
line costs ~45 µs. The WAD stays in the W25Q128 (read-only, same cache). This is the heaviest
software of the ten — an estimate of 10–20 fps, not measured. Display: each frame 320 × 200 × 2 B by
SPI2 DMA at 25 MHz: ~41 ms, so the display limits it to ~24 fps; drawing in the background while
the next frame renders.

## Power

| State | Current (estimate) |
|---|---|
| STM32F411 at 100 MHz | ~25 mA |
| PSRAM + flash active | ~15 mA |
| ST7789 2.0" with backlight | ~40 mA |
| MAX98357A playing | 20–200 mA from the cell |
| **Typical** | **~110 mA at 3.7 V** |

A 3000 mAh 18650 → **~25 h**. TP4056 charges it at 1 A in ~3.5 h.

## Firmware: build and flash

- STM32CubeF4 (HAL/LL) or libopencm3, `arm-none-eabi-gcc -mcpu=cortex-m4 -mfpu=fpv4-sp-d16
  -mfloat-abi=hard`; a linker script with the 512 KB flash and 128 KB SRAM; the WAD written once into
  the W25Q128 by the firmware from USB (a DFU alternate setting) or by a programmer.
- Flash: `dfu-util -a 0 -s 0x08000000:leave -D doom.bin` (BOOT held at reset; the ROM's DFU needs the
  25 MHz HSE, which is fitted), or `st-flash write doom.bin 0x8000000` / `openocd -f
  interface/stlink.cfg -f target/stm32f4x.cfg` on the SWD header.

## Bill of materials

Rough cost at 10 pieces (estimates): STM32F411CEU6 $2.50 · APS6404L $1.20 · W25Q128 $1.00 · 25 MHz
crystal $0.15 · ST7789 2.0" module $4.00 · MAX98357A $0.90 · TP4056 $0.10 · DW01A + FS8205A $0.15 ·
TLV75533 $0.20 · Keystone 1042 $2.50 · USB-C $0.40 · buttons $0.45 · passives $1.20 · 4-layer PCB
$3.50 · 18650 cell $4.00 → **about $23**.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 1 | BT1 |  | 1042 | BatteryHolder_Keystone_1042_1x18650 |
| 2 | C1, C2 | 15pF |  | C_0402_1005Metric |
| 9 | C3, C4, C5, C8, C11, C13, C14, C16, C19 | 100nF |  | C_0402_1005Metric |
| 2 | C6, C7 | 4.7uF |  | C_0805_2012Metric |
| 3 | C9, C20, C21 | 1uF |  | C_0603_1608Metric |
| 2 | C10, C12 | 1uF |  | C_0402_1005Metric |
| 3 | C15, C17, C18 | 10uF |  | C_0805_2012Metric |
| 1 | D1 | tvs 5V |  | D_SOD-323 |
| 1 | D2 | red |  | LED_0603_1608Metric |
| 1 | D3 | green |  | LED_0603_1608Metric |
| 1 | DS1 |  | MSP2008 2.0in 240x320 IPS SPI module (ST7789) | TFT_2.0in_ST7789_Socket_1x08_P2.54mm |
| 1 | J1 | PinHeader_1x05_P2.54mm_Vertical |  | PinHeader_1x05_P2.54mm_Vertical |
| 1 | J2 | USB_C_Receptacle_USB2.0_16P |  | USB_C_Receptacle_USB2.0_16P |
| 1 | J5 |  | S2B-PH-K-S(LF)(SN) | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | Q1 |  | FS8205A | TSSOP-8_4.4x2.93mm_P0.65mm |
| 6 | R1, R3, R4, R5, R6, R7 | 10kohm |  | R_0402_1005Metric |
| 1 | R2 | 100kohm |  | R_0402_1005Metric |
| 1 | R8 | 634kohm |  | R_0402_1005Metric |
| 2 | R9, R10 | 5.1kohm |  | R_0402_1005Metric |
| 1 | R11 | 1ohm |  | R_0603_1608Metric |
| 1 | R12 | 1.2kohm |  | R_0402_1005Metric |
| 2 | R13, R15 | 1kohm |  | R_0402_1005Metric |
| 1 | R14 | 100ohm |  | R_0402_1005Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW9 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | SW10 | SW_Tactile_SPST_5.1x5.1mm_SMD |  | SW_Tactile_SPST_5.1x5.1mm_SMD |
| 1 | U1 |  | STM32F411CEU6 | QFN-48-1EP_7x7mm_P0.5mm_EP5.6x5.6mm |
| 1 | U2 |  | APS6404L-3SQR-SN | SOIC-8_3.9x4.9mm_P1.27mm |
| 1 | U3 |  | W25Q128JVSIQ | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U4 |  | TLV75533PDBVR | SOT-23-5 |
| 1 | U5 |  | MAX98357AETE+T | QFN-16-1EP_3x3mm_P0.5mm_EP1.1x1.1mm |
| 1 | U6 |  | TP4056 | SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.413x3.302mm |
| 1 | U7 |  | DW01A | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U8 | ESD 2-line |  | SOT-23-6 |
| 1 | Y1 | 25MHz 10pF |  | Crystal_SMD_3225-4Pin |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** **ARMv7E-M** = the ARMv6-M core extended with Thumb-2 (32-bit encodings, IT blocks,
  bit-field, saturating and DSP instructions, hardware divide) and the **FPv4-SP FPU** (S0–S31, FPSCR,
  lazy stacking on exception entry). Exception model as v6-M plus priority grouping, BASEPRI, fault
  status registers.
- **Interrupt controller:** NVIC with 86 lines, SysTick, SCB (VTOR).
- **Boot:** BOOT0 strap: flash at 0x0800_0000 aliased to 0x0, or the **system memory ROM** (USART/USB
  DFU bootloader, ST's code: modelled by behaviour, not by its image); image format: a raw binary with
  the vector table first.
- **Clocks (RCC):** HSE 25 MHz → PLL (M/N/P/Q) → 100 MHz SYSCLK, 48 MHz for USB; flash wait states
  (FLASH_ACR) that the firmware sets before raising the clock.
- **Peripherals:** GPIO with AFR **pin-mux tables** (AF5 SPI1/2, AF6 I2S3, AF1 TIM2) — data from ST's
  open pin XML/SVD; SPI1/SPI2 with **DMA2/DMA1 streams**; I2S3 with its PLLI2S; TIM2 PWM; USB OTG FS
  (device, DFU class) if flashing through USB is simulated; SysTick.
- **Board:** APS6404L and W25Q128 models behind chip selects (the link needs CS), ST7789 model,
  MAX98357A as an I2S sink to WebAudio, 18650 cell with DW01A thresholds and TP4056 CC/CV charge.

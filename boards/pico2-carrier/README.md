# 06 · pico2-carrier — the reference device as an orderable carrier

PCBKit's reference Doom device, made orderable: a carrier for any Pico-2-footprint module — the
Raspberry Pi Pico 2 (RP2350A, 4 MB flash, 520 KB SRAM) or, for the full game, the pin-compatible
**Pimoroni Pico Plus 2** (RP2350B, 16 MB flash, **8 MB PSRAM** on QMI chip select 1 = GPIO47) — with
the same 2.8" ILI9341 touch TFT (MSP2807) and ten buttons as the reference, plus a PCM5102A DAC and a
headphone jack. Power is USB only (the module's own USB connector and buck-boost): the simplest power
design of the ten; a power bank makes it portable.

| | |
|---|---|
| Processor | RP2350 on a module: 2 × **Cortex-M33** (ARMv8-M Mainline, DSP, single-precision FPU, TrustZone) **or** 2 × **Hazard3** (RV32IMAC + Zba/Zbb/Zbs/Zbkb/Zcb/Zcmp), chosen by the image; 150 MHz; 520 KB SRAM; QSPI flash (and on the Plus 2 the PSRAM) through the QMI's 16 KB XIP cache |
| Display | 2.8" 240 × 320 ILI9341 with XPT2046 resistive touch on SPI0 (GP18 SCK, GP19 MOSI, GP16 MISO, GP17 CS, GP20 DC, GP21 RESET, GP22 backlight; touch CS GP13, IRQ GP14) |
| Input | 10 × 6 mm tact switches, as the reference: d-pad GP0–GP3, fire GP4, use GP5, strafe L/R GP6/GP7, menu GP8, enter GP9 |
| Sound | PIO I2S (GP10 BCK, GP11 LRCK, GP12 DIN) → PCM5102A (hardware mode, PLL from BCK, mute on GP15) → 470 Ω / 2.2 nF → 3.5 mm stereo jack |
| Power | USB 5 V into the module (VBUS → its diode → VSYS → RT6150/TPS63000-class buck-boost → 3.3 V, up to 300 mA for the carrier) |
| Programming | the module's USB: BOOTSEL + plug → UF2 (RP2350's ROM accepts ARM and RISC-V images); SWD on the module's debug pads |
| Board | 160 × 62 mm, 2 layers; front: display, ten buttons; back: module (USB at the top edge), DAC, jack |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 0 warnings, 0 unrouted nets; 35 components, 33 nets, 286 track segments (2024 mm), 29 vias.
<!-- /STATUS -->

## Why it runs Doom

With the Pico Plus 2: 8 MB of PSRAM mapped at 0x1100_0000 through the XIP cache, so an ordinary
Doom port (Chocolate Doom's engine with a small platform layer) runs with its zone heap in PSRAM and
the full DOOM1.WAD in flash, at 150 MHz on one core with the second core driving the display:
the reference implementation PCBKit's machine work aims to simulate. With the plain Pico 2 (no
PSRAM), rp2040-doom's RP2350 build applies (520 KB SRAM is twice the RP2040's). The ILI9341 takes
320 × 240 RGB565 by SPI DMA at 62.5 MHz: 153 KB, ~20 ms a frame.

## Power

| State | Current from USB 5 V (estimate) |
|---|---|
| RP2350 at 150 MHz + PSRAM | ~40 mA at 3.3 V |
| ILI9341 2.8" with backlight | ~60–80 mA |
| PCM5102A | ~20 mA |
| **Total** | **~130 mA at 3.3 V ≈ 100 mA from 5 V** |

A 5000 mAh power bank (≈ 18 Wh) → **~30 h**. Many power banks switch off below ~50–100 mA; this
board stays above that while the backlight is on.

## Firmware: build and flash

- Pico SDK 2.x: `cmake -DPICO_BOARD=pimoroni_pico_plus2_rp2350 -DPICO_PLATFORM=rp2350-arm-s` (Arm,
  `arm-none-eabi-gcc`) or `-DPICO_PLATFORM=rp2350-riscv` (Hazard3, `riscv32-unknown-elf-gcc` with
  `-march=rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb`); the image carries an **IMAGE_DEF block** that
  the boot ROM reads to pick the architecture.
- Flash: BOOTSEL + USB → copy the UF2 (family ID `rp2350-arm-s` or `rp2350-riscv`); `picotool load
  -x doom.uf2`; or SWD with a Debug Probe.
- In PCBKit: this is the board the RP2350 machine model is being written against; `firmware = "doom"`
  on the module once the RP2350 SoC is registered (`docs/adr/0025-machine-framework.md`).

## Bill of materials

Rough cost at 10 pieces (estimates): Pimoroni Pico Plus 2 $13 (Pico 2: $5) · 2 × 20-pin sockets
$0.60 · ILI9341 2.8" touch module $7.00 · PCM5102A $2.00 · jack $0.60 · 10 buttons $0.50 · passives
$0.60 · PCB $2.00 → **about $26** with the Plus 2.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 4 | C1, C3, C5, C9 | 100nF |  | C_0402_1005Metric |
| 3 | C2, C4, C6 | 10uF |  | C_0805_2012Metric |
| 2 | C7, C8 | 2.2uF |  | C_0805_2012Metric |
| 2 | C10, C11 | 2.2nF |  | C_0402_1005Metric |
| 1 | DS1 |  | MSP2807 2.8in 240x320 SPI TFT module with touch (ILI9341) | TFT_2.8in_ILI9341_Socket_1x14_P2.54mm |
| 1 | J1 |  | SJ1-3525N | Jack_3.5mm_SameSky_SJ1-352XN_Horizontal |
| 4 | R1, R2, R3, R4 | 10kohm |  | R_0603_1608Metric |
| 2 | R5, R6 | 470ohm |  | R_0402_1005Metric |
| 10 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | U1 |  | SC1631 | Raspberry_Pi_Pico_2_Socket_2x20_P2.54mm_W17.78mm |
| 1 | U2 |  | PCM5102APWR | TSSOP-20_4.4x6.5mm_P0.65mm |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Cores:** Hazard3 (RV32IMAC + the bit-manipulation and Zcb/Zcmp extensions, machine and user modes,
  PMP) — the existing RV32IMAC core plus extensions; **or** Cortex-M33 (ARMv8-M Mainline: Thumb-2 as
  for M4, FPU, stack limit registers, the security extension the SDK sets up in Secure state).
- **Interrupt controllers:** Hazard3's **Xh3irq** (an external-interrupt controller in custom CSRs:
  `meiea`, `meipa`, `meinext`… — the `Bus::csr` hook exists for exactly this) and the M33's NVIC.
- **Boot ROM:** RP2350's ROM searches flash for an IMAGE_DEF in the first 4 KB, chooses ARM or
  RISC-V, sets up XIP, handles BOOTSEL (USB UF2 + PICOBOOT) and the OTP; a model needs at least the
  image-definition parser and the XIP setup the ROM leaves behind.
- **Memory:** the **QMI** with two chip selects (flash on CS0, PSRAM on CS1/GPIO47), its timing and
  the 16 KB XIP cache; PSRAM writable through the cache.
- **Clocks:** XOSC 12 MHz, PLL_SYS 150 MHz, clk_peri, clk_hstx; the TICKS block for timers.
- **Peripherals:** PL022 SPI0 + **DMA** (the display), **PIO** (I2S), TIMER0/1, IO_BANK0/PADS with
  the RP2350's funcsel table, SIO (FIFOs, spinlocks, the RISC-V/ARM-agnostic doorbells), RESETS,
  the watchdog's scratch registers (the ROM's reboot path).
- **Board:** ILI9341 + XPT2046 models on one SPI bus with two chip selects, the PCM5102A as an I2S
  sink, and USB 5 V as an ideal source.

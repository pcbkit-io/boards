# 02 · pico-oled — a through-hole Doom kit on a Raspberry Pi Pico

A beginner's kit: everything plugs in or is soldered through holes except one SOD-123 diode and one
resistor. A Raspberry Pi Pico (RP2040) on sockets behind a 2.42" monochrome OLED, eight buttons, a
piezo and three AA cells. The game is kilograham's **rp2040-doom** (the full shareware game in the
Pico's 2 MB flash, 264 KB of RAM) with its VGA/DVI back end replaced by a 1-bit OLED back end.

| | |
|---|---|
| Processor | Raspberry Pi Pico (SC0915): RP2040, 2 × Arm **Cortex-M0+** (ARMv6-M, Thumb), up to 133 MHz (rp2040-doom runs it at 270 MHz overclocked), 264 KB SRAM, 2 MB QSPI flash on the module |
| Display | 2.42" 128 × 64 OLED module, SSD1309, 4-wire SPI on SPI0 (GP18 SCK, GP19 MOSI, GP17 CS, GP20 D/C, GP21 RES) |
| Input | 8 × 6 mm tact switches to GND: d-pad GP2–GP5, fire GP6, use GP7, strafe GP8, menu GP9 (internal pull-ups) |
| Sound | passive piezo GPC12075YB on GP15 (PWM) through 100 Ω |
| Power | 3 × AA (4.5 V fresh, ~3.0 V empty) → JS102011SAQN slide switch → B5819W Schottky → Pico VSYS; the Pico's RT6150 buck-boost makes 3.3 V from 1.8–5.5 V |
| Programming | the Pico's micro-USB: hold BOOTSEL, plug in, copy `doom.uf2` to the RPI-RP2 drive |
| Board | 150 × 62 mm, 2 layers; front: OLED, buttons, switch; back: Pico (USB end at the top edge), piezo, battery leads |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 0 warnings, 0 unrouted nets; 19 components, 20 nets, 69 track segments (783 mm), 7 vias.
<!-- /STATUS -->

## Why it runs Doom

rp2040-doom (Graham Sanderson, 2021) already did the hard part: Doom's 4 MB WAD compressed into a
custom "WHD" format (the shareware episode in about 1.1 MB), static tables in flash, the zone heap
cut to fit 264 KB with in-place decompression, the renderer split over both cores. Its video back end
scans out 320 × 200 through PIO. For this board the scan-out is replaced by: core 1 downsamples the
8-bit frame to 128 × 64, Floyd–Steinberg dithers it to 1 bit, and sends 1 KB per frame over SPI at
10 MHz (0.8 ms): the display is never the bottleneck. Expected speed: the rp2040-doom frame rate
(about 35 fps at 270 MHz per its author; an estimate here, not measured).

## Power

| State | Current from the cells (estimate) |
|---|---|
| RP2040 at 270 MHz, both cores busy | ~50 mA at 3.3 V |
| SSD1309, half the pixels lit | ~15–25 mA |
| **Total** | **~70 mA at 3.3 V ≈ 0.23 W → ~60 mA from 4.5 V after the buck-boost** |

Three alkaline AA (~2.5 Ah each at this drain, in series ≈ 3.75 V average, ~9 Wh) → **about 30 h**
of play. USB power works at the same time: VBUS reaches VSYS through the Pico's own diode, the
Schottky D1 keeps it out of the cells.

## Storage

Everything is in the Pico's 2 MB flash: code and the WHD-compressed shareware WAD (the
registered/Ultimate WAD does not fit; use a Pico with 16 MB, e.g. Pimoroni Pico Plus 2, or variant 04).

## Firmware: build and flash

- Pico SDK 2.x + `arm-none-eabi-gcc` 13; `cmake -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040`; rp2040-doom's
  `whd_gen` tool converts DOOM1.WAD to the compressed format linked into the image.
- Flash: BOOTSEL + USB → drag `doom.uf2` (the boot ROM's mass-storage loader), or SWD with a
  Raspberry Pi Debug Probe on the Pico's three SWD pads.

## Bill of materials

Rough cost at 10 pieces (estimates): Pico $4.00 · 2 × 20-pin sockets $0.60 · SSD1309 2.42" module
$9.00 · 8 buttons $0.40 · piezo $0.30 · switch $0.30 · 3×AA holder $0.80 · PCB $1.50 → **about $17**.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 1 | BT1 |  | 3xAA battery holder with leads | BatteryLeads_2x_D1.1mm_P3.81mm |
| 1 | BZ1 |  | GPC12075YB-5V4000 | Buzzer_D12.0mm_H7.5mm_P6.50mm |
| 1 | D1 | schottky | B5819W | D_SOD-123 |
| 1 | DS1 |  | MSP242X 2.42in 128x64 SPI module (SSD1309) | OLED_2.42in_SPI_Socket_1x07_P2.54mm |
| 1 | R1 | 100ohm |  | R_0805_2012Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW9 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | U1 |  | SC0915 | Raspberry_Pi_Pico_Socket_2x20_P2.54mm_W17.78mm |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** ARMv6-M (exists) ×2 with the SIO's inter-core FIFOs, spinlocks and the hardware divider
  and interpolators (rp2040-doom uses both cores and the interpolators).
- **Boot:** the RP2040 boot ROM (BSD-3, `raspberrypi/pico-bootrom`) and the boot2 stage that
  configures the SSI for XIP; UF2 as the image format (or our direct-XIP profile, which skips both).
- **Memory:** XIP flash behind the SSI with its 16 KB cache (cycle cost of a miss matters for speed).
- **Clocks:** XOSC, PLL_SYS to 270 MHz, clk_peri; the flash divider.
- **Peripherals:** PL022 SPI (SPI0 TX FIFO), DMA (the OLED transfer), PWM (piezo), TIMER, IO_BANK0
  and PADS (pull-ups), RESETS.
- **Board components:** an SSD1309 model (command set, page addressing, 128 × 64 GDDRAM → the frame
  the 3D board shows), a piezo as a sound source driven by a PWM pin, AA cells as a discharge curve
  behind a diode into the module's buck-boost.

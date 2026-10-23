# 02 · pico-oled — a through-hole Doom kit on a Raspberry Pi Pico

A Raspberry Pi Pico (RP2040) on sockets behind a 2.42" monochrome OLED, eight buttons, a piezo and
two 18650 Li-ion cells it charges from the Pico's micro-USB. JLCPCB assembles everything but the two
modules, which plug into the sockets it solders. The game is kilograham's **rp2040-doom** (the full shareware game in the
Pico's 2 MB flash, 264 KB of RAM) with its VGA/DVI back end replaced by a 1-bit OLED back end.

| | |
|---|---|
| Processor | Raspberry Pi Pico (SC0915): RP2040, 2 × Arm **Cortex-M0+** (ARMv6-M, Thumb), up to 133 MHz (rp2040-doom runs it at 270 MHz overclocked), 264 KB SRAM, 2 MB QSPI flash on the module |
| Display | 2.42" 128 × 64 OLED module, SSD1309, 4-wire SPI on SPI0 (GP18 SCK, GP19 MOSI, GP17 CS, GP20 D/C, GP21 RES) |
| Input | 8 × 6 mm tact switches to GND: d-pad GP2–GP5, fire GP6, use GP7, strafe GP8, menu GP9 (internal pull-ups) |
| Sound | passive piezo GPC12075YB on GP15 (PWM) through 100 Ω |
| Power | 2 × 18650 Li-ion in parallel (1S2P, 3.0–4.2 V) behind reverse-cell (AO3401A) and DW01A + FS8205A protection; BQ24075 charger with power path from the Pico's USB; SYS → 1N5819HW → Pico VSYS; the Pico's RT6150 buck-boost makes 3.3 V |
| Programming | the Pico's micro-USB: hold BOOTSEL, plug in, copy `doom.uf2` to the RPI-RP2 drive (the same port charges) |
| Board | 150 × 100 mm, 2 layers, four M3 holes; front: OLED, buttons, charger, on/off switch, test pads; back: Pico (USB end at the top edge), two 18650 holders, piezo |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 0 warnings, 0 unrouted nets. 43 components, 37 nets, 339 track segments (1685 mm), 24 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partial. Nothing to fix, 1 to consider, 12 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
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

Two 18650 Li-ion cells in Keystone 1042 holders on the back, in parallel (1S2P, 3.0–4.2 V, about
5–7 Ah together). From the cells to the Pico:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). It cannot help one cell reversed against the other: in parallel
  they would short each other, so check both against the silkscreen's + before closing a case.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack. Flat-top unprotected 18650 cells fit
  the holders.
- **Charger with power path:** a TI BQ24075 from the Pico's VBUS, in USB500 mode (at most 500 mA
  from the port), 800 mA fast charge (RISET 1.13 kΩ), input limit set by RILIM 1.18 kΩ, safety timers
  off (two cells in parallel charge longer than its longest timer; termination is by current), TS
  tied to 10 kΩ (no thermistor). Its output SYS runs the board from USB when one is plugged in and
  from the cells otherwise; SYS reaches the Pico's VSYS through a 1N5819HW Schottky, beside the
  Pico's own VBUS diode.
- **On/off:** the slide switch on the left edge pulls the Pico's 3V3_EN low (its buck-boost off; the
  charger still charges with the switch off).

| State | Current from the cells (estimate) |
|---|---|
| RP2040 at 270 MHz, both cores busy | ~50 mA at 3.3 V |
| SSD1309, half the pixels lit | ~15–25 mA |
| **Total** | **~70 mA at 3.3 V ≈ 0.25 W with losses → ~65 mA from 3.7 V** |

Two 3000 mAh cells → **about 90 h** of play (estimate). In USB500 mode the charger takes at most
500 mA from the port, the board's own draw included, so the cells charge at somewhat under 500 mA
(the 800 mA ISET setting is the ceiling, not reached): about 13 h for two empty cells, longer while
playing.

## Charging

Plug a USB charger or computer into the Pico's micro-USB. The red LED (CHG) is on while the cells
charge and goes out when they are full; the green LED (USB) is on while USB power is present. The
board runs from USB while it charges, switched on or off. Insert the cells with + at the end the
back's silkscreen marks.

## Modules plugged in by hand

The assembly solders two 1x20 female sockets and one 1x7 female socket; the modules are bought
separately and plugged in:

- **U1: Raspberry Pi Pico** (SC0915, with its male headers soldered on: the Pico H has them),
  on the back, components facing the board, the USB end at the top edge.
- **DS1: 2.42" 128 × 64 SSD1309 OLED module, SPI** (7 pins: GND, VCC, SCL, SDA, RES, DC, CS),
  on the front. Modules are sold set to I2C by default on some listings: buy the SPI version (or
  move its resistors as its seller describes).

## Storage

Everything is in the Pico's 2 MB flash: code and the WHD-compressed shareware WAD (the
registered/Ultimate WAD does not fit; use a Pico with 16 MB, e.g. Pimoroni Pico Plus 2, or variant 04).

## Firmware: build and flash

- Pico SDK 2.x + `arm-none-eabi-gcc` 13; `cmake -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040`; rp2040-doom's
  `whd_gen` tool converts DOOM1.WAD to the compressed format linked into the image.
- **Flashing on arrival:** hold the Pico's BOOTSEL button and plug its micro-USB into a computer:
  it appears as the RPI-RP2 drive; copy `doom.uf2` onto it and the Pico reboots into the game. No
  programmer and nothing on the board to configure. (Or SWD with a Raspberry Pi Debug Probe on the
  Pico's three SWD pads.)

## Bill of materials

Rough cost at 10 pieces (estimates): Pico $4.00 · 2 × 20-pin sockets $0.60 · SSD1309 2.42" module
$9.00 · 8 buttons $2.30 · piezo $0.60 · switch $0.10 · charger, protection and passives $1.80 · two 18650 holders $9.80 · PCB $1.50 → **about $30** without the cells.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 1 | BZ1 | GPC12075YB-5V4000 | C252948 | Buzzer_D12.0mm_H7.5mm_P6.50mm |
| 1 | C1 | 100nF | C1525 | C_0402_1005Metric |
| 1 | D1 | 1N5819HW-7-F | C82544 | D_SOD-123 |
| 1 | DS1 | Female header 1x7 2.54 mm | C2932672 | OLED_2.42in_SPI_Socket_1x07_P2.54mm |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 1 | R1 | 100ohm | C17408 | R_0805_2012Metric |
| 1 | R2 | 100kohm | C25741 | R_0402_1005Metric |
| 1 | R3 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R4 | 1kohm | C11702 | R_0402_1005Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW9 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 2 | U1 | Female header 1x20 2.54 mm | C2905423 | Raspberry_Pi_Pico_Socket_2x20_P2.54mm_W17.78mm |
| 1 | U2 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U3 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
| 1 | rTs | 10kohm | C25744 | R_0402_1005Metric |
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
  the 3D board shows), a piezo as a sound source driven by a PWM pin, two 18650 cells as a discharge
  curve behind the charger's power path and a diode into the module's buck-boost.

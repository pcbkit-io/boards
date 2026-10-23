# 09 · s3-rgb — a 4.3" RGB-panel Doom on the ESP32-S3's LCD_CAM

The big-screen variant: the ESP32-S3 scans a **raw RGB TFT** (no controller, no frame memory) out of
a frame buffer in its octal PSRAM with the LCD_CAM peripheral and DMA — the architecture of the
popular ESP32-S3 "4.3-inch" boards. Ten buttons on an I2C expander (the pins all go to the panel),
stereo sound through a DAC and an amplifier, and two 18650 cells it charges from its USB-C port.
JLCPCB assembles everything; the panel is taped on and plugged in by hand.

| | |
|---|---|
| Processor | ESP32-S3-WROOM-1-N16R8 (2 × Xtensa LX7 240 MHz, 512 KB SRAM, 16 MB flash, 8 MB octal PSRAM) |
| Display | Newhaven **NHD-4.3-480272EF-ASXN**: 4.3" 480 × 272, 24-bit RGB parallel, DE mode, DCLK 9–15 MHz; driven as RGB565 (R3–R7 IO1/2/4/5/6, G2–G7 IO7/15/16/17/18/8, B3–B7 IO9–IO13; PCLK IO42, DE IO40, VSYNC IO41, HSYNC IO39; the low bits grounded) through a 40-pin 0.5 mm FFC connector (Molex 54132-4062) on the back at the top edge: the panel's cable folds over the edge into it |
| Backlight | 12 white LEDs, 25.6 V at 40 mA: **MT3608 boost as a constant-current driver** from SYS (FB regulates 0.6 V over 15 Ω), EN/PWM on IO46 (a strap that must be low at reset: 100 kΩ pull-down) |
| Input | 10 tact switches on an **MCP23017** (I2C 0x20, IO14 SDA, IO38 SCL), interrupt on change to IO3 |
| Sound | I2S (IO47 BCK, IO48 LRCK, IO21 DIN) → **PCM5102A** DAC → **PAM8403** stereo class-D from SYS (SHDN on 3.3 V: asleep when switched off) → two 4–8 Ω speakers on JST PH plugs at the left and right edges |
| Power | **2 × 18650 Li-ion in parallel** (1S2P) behind a reverse-cell P-FET (AO3401A) and DW01A + FS8205A protection; USB-C (1 A PTC fuse, TVS, ESD array) → **BQ24075** charger with power path → SYS for the backlight driver and the amplifier → **TLV75533** low-dropout regulator for 3.3 V; on/off slide switch on the regulator's EN |
| Programming | USB-C D+/D− to the S3's native USB (IO19/IO20): esptool over USB-Serial/JTAG; RESET (EN) and BOOT (IO0) buttons on the back |
| Board | 182 × 80 mm, **4 layers**, four M3 holes; front: the panel on tape (outline in the silkscreen), buttons, the power corner (USB-C at the top edge, charger, CHG/USB LEDs, on/off switch); back: two 18650 holders along the bottom edge, module (antenna past the top edge), FFC connector at the top edge, protection, regulator, backlight driver, expander, audio, test pads |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 1 warning (diff-pair-uncoupled), 0 unrouted nets; `issues` has 1 of 1. 97 components, 85 nets, 882 track segments (3572 mm), 99 vias.
`analysis.run { lens: "ready" }`: Typical scenario from the parts' datasheets. Nothing to fix, 3 to consider, 14 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
<!-- /STATUS -->

What `check` and the ready lens leave to weigh, and why each is acceptable:

| Warning | Why it is acceptable |
|---|---|
| `diff-pair-uncoupled`: DP / DN about 21 mm uncoupled (128 mm coupled), over the default 10 mm | USB full speed (12 Mbit/s): the connector is at the top left, the module at the top right; the uncoupled stretch is at the two ends, harmless at 4–20 ns edges |
| GND's copper heats up by 28 °C (1.5 A on the thinnest GND track) | the lens rates one 0.15 mm GND track alone; GND is also a pour on the top, inner and bottom layers that carries the return current beside it (the Current lens notes the pour runs it cooler) |
| 6 heat sources need exact thermal parameters | the regulator, charger, boost, amplifier, DAC and module dissipate well under 0.5 W each in this design; the estimate uses generic package limits, as for every board here |

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/doom-handheld/tree/main/boards/s3-rgb, open the
Laboratory and press **Build & run**: the page builds `doom/` for the ESP32-S3's Xtensa LX7 with
Espressif's LLVM (with `DOOM1.WAD` linked into its flash), boots it the way the chip's boot ROM
boots an image from flash, and shows on the panel the frames LCD_CAM scans out of the PSRAM. The
buttons reach the program through the MCP23017, as on the board. Play with the keyboard (arrows,
F fire, Space use, comma/period strafe, M menu, Enter) or a gamepad (`input.js`).

## Why it runs Doom (and what the screen costs)

Doom is the same port as variant 08 (the program and the WAD in place in flash, the zone heap,
screens and visplanes in the octal PSRAM, here at 40 MHz). The difference is the display: the
LCD_CAM peripheral in RGB mode streams a 480 × 272 RGB565 frame buffer from the PSRAM by GDMA
without end, at a 10 MHz pixel clock with the Newhaven sheet's porches (HSYNC 1 + 43 + 480 + 2,
VSYNC 1 + 12 + 272 + 4): 66 Hz, 261 KB a frame, about 17 MB/s of the PSRAM's ~40 MB/s at 40 MHz
octal DDR (estimate). Each Doom frame is scaled 1.36 times through the palette into the middle
435 × 272 of that buffer and written back from the data cache (the DMA reads the PSRAM, not the
cache). The panel never stops refreshing, which is why this design matters to the simulator: the
display is a **timed bus master**, not a sequence of SPI commands. The ten buttons are read from
the MCP23017 over I2C0 (400 kHz) once a tic. Measured in PCBKit's simulator: the title by 4 s of
device time, the demo by 9 s; 16 s simulated in 12 s.

## Power

Two 18650 Li-ion cells in Keystone 1042 holders on the back, end to end along the bottom edge, in
parallel (1S2P, 3.0–4.2 V, about 5–7 Ah together). From the cells to the loads:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). It cannot help one cell reversed against the other: in parallel
  they would short each other, so check both against the silkscreen's + before closing a case.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack. Flat-top unprotected 18650 cells fit
  the holders.
- **USB-C input:** 5.1 kΩ on CC1 and CC2 (a sink: 5 V from any charger or computer), a 1 A
  resettable fuse, a 5 V TVS on VBUS and a USBLC6 on D+/D−.
- **Charger with power path:** a TI BQ24075 in USB500 mode (at most 500 mA from the port), 800 mA
  fast charge (RISET 1.13 kΩ), input limit by RILIM 1.18 kΩ, safety timers off (two cells in
  parallel charge longer than its longest timer; termination is by current), TS tied to 10 kΩ (no
  thermistor). Its output SYS runs the board from USB when one is plugged in and from the cells
  through its battery FET otherwise; with more load than USB gives, the cells make up the rest.
- **SYS** feeds the MT3608 backlight driver and the PAM8403 directly (no 5 V rail: both run from
  2.8–5.5 V), and a **TLV75533** (500 mA, 238 mV dropout at 500 mA, about 100 mV at this board's
  ~200 mA) for 3.3 V. It holds 3.3 V down to about 3.4 V of cell; below that the rail follows the
  cells down to 3.0 V, where the ESP32-S3 (3.0–3.6 V) still runs and the cells have under 5 % left.
  The AMS1117 it replaces needs 1.1 V of headroom: straight from the cells it would never make 3.3 V.
- **On/off:** the slide switch on the left edge drives the regulator's EN (100 kΩ holds it low
  between positions). Off, 3.3 V is gone: the module, panel logic, expander and DAC are unpowered,
  the backlight driver's EN is pulled low and the amplifier's SHDN (on 3.3 V) puts it to sleep; the
  cells still charge.

| State | Current (estimate) |
|---|---|
| ESP32-S3 + PSRAM streaming the panel | ~120 mA at 3.3 V |
| Panel logic | 25 mA at 3.3 V |
| Backlight 25.6 V × 40 mA ≈ 1.0 W | ~320 mA from 3.7 V at 85 % |
| PAM8403, moderate volume | ~100 mA |
| **Typical** | **≈ 2.3 W → ~620 mA at 3.7 V** |

Two 3000 mAh cells → **about 9 h** of play (estimate). The TLV75533 burns (3.7 − 3.3) × 0.15 A ≈
0.06 W from the cells (0.3 W from USB).

## Charging

Plug a USB-C charger or computer into the port at the top left. The red LED (CHG) is on while the
cells charge and goes out when they are full; the green LED (USB) is on while USB power is
present. Switched off, the cells take what the port allows in USB500 mode, just under 500 mA: two empty
3000 mAh cells take about 13 h (overnight). Playing
while plugged in, the board takes about 600 mA, more than USB500 mode draws, so the cells hold or
slowly discharge instead of charging. Insert the cells with + at the end the back's silkscreen marks.

## Modules plugged in by hand

- **DS1: the panel**, Newhaven NHD-4.3-480272EF-ASXN (or a generic 4.3" 480 × 272 40-pin RGB
  panel with the same pinout), bought separately. Tape it to the front inside its silkscreen
  outline with double-sided foam tape, its FPC toward the top edge; fold the FPC over the top edge
  and lock it into J1 on the back (contacts down, Molex 54132 is bottom contact). The firmware
  scans out the frame upside-down if the panel's FPC side is its bottom.
- **Two speakers**, 4–8 Ω, 1–3 W, on 2-pin JST PH plugs (J5 left, J6 right).
- **Two 18650 cells.**

## Firmware: build and flash

- **Build:** in PCBKit, the Laboratory's **Build & run** builds `doom/` (Xtensa LX7, Espressif's
  LLVM, the kit's start-up, LCD_CAM/GDMA and I2C drivers); then export the firmware
  (`export.run { format: "firmware" }`): `U1.bin`, the whole flash from offset 0. No 2nd-stage
  bootloader or partition table: the ROM boots the image directly.
- **Flashing on arrival:** switch the board on and plug the USB-C port into a computer: the
  ESP32-S3's built-in USB-Serial/JTAG appears as a serial port (no driver on Linux and macOS; on
  Windows 10+ the class driver); `esptool.py --chip esp32s3 write_flash 0x0 U1.bin` resets it into
  the ROM loader by itself. A fresh module's flash is empty and its ROM loader answers at once. If
  firmware ever takes the USB pins over, hold BOOT (IO0) on the back, press RESET (EN), release
  BOOT: the ROM loader is back. Press RESET: Doom starts.

## Bill of materials

Rough cost at 10 pieces (estimates): ESP32-S3-WROOM-1-N16R8 $4.50 · NHD-4.3-480272EF-ASXN $35 (a
generic 4.3" 480 × 272 40-pin panel: $8) · Molex 54132-4062 $1.00 · MT3608 $0.15 · MCP23017 $1.20 ·
PCM5102A $2.00 · PAM8403 $0.15 · BQ24075, protection and TLV75533 $1.60 · USB-C $0.40 · buttons
$3.40 · passives $1.80 · two 18650 holders $9.80 · 4-layer PCB $6.00 · 2 speakers $2.00 → **about
$71** with the Newhaven panel, ~$44 with a generic one, without the cells.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 1 | C1 | 10uF | C19702 | C_0603_1608Metric |
| 1 | C2 | 22uF | C45783 | C_0805_2012Metric |
| 1 | C3 | 4.7uF | C29823 | C_1206_3216Metric |
| 2 | C4, C22 | 22uF | C12891 | C_1206_3216Metric |
| 7 | C5, C7, C8, C10, C12, C16, C26 | 100nF | C1525 | C_0402_1005Metric |
| 5 | C6, C19, C20, C21, C23 | 1uF | C52923 | C_0402_1005Metric |
| 3 | C9, C11, C13 | 10uF | C15850 | C_0805_2012Metric |
| 2 | C14, C15 | 2.2uF | C87994 | C_0805_2012Metric |
| 2 | C17, C18 | 2.2nF | C1531 | C_0402_1005Metric |
| 2 | C24, C25 | 470nF | C47339 | C_0402_1005Metric |
| 3 | C33, C34, cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | D1 | 1N5819HW-7-F | C82544 | D_SOD-123 |
| 1 | D6 | tvs 5V | C918844 | D_SOD-323 |
| 1 | F1 | PTC 1000mA | C20800 | Fuse_1206_3216Metric |
| 1 | J1 | 54132-4062 | C505102 | Conn_FFC-FPC_1x40_P0.50mm_Horizontal |
| 1 | J3 | USB_C_Receptacle_USB2.0_16P | C165948 | USB_C_Receptacle_USB2.0_16P |
| 2 | J5, J6 | S2B-PH-K-S(LF)(SN) | C173752 | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | L1 | 22uH | C83472 | L_Power_4x4mm |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 4 | R1, R3, R4, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 3 | R2, R11, R14 | 100kohm | C25741 | R_0402_1005Metric |
| 2 | R5, R6 | 4.7kohm | C25900 | R_0402_1005Metric |
| 2 | R7, R8 | 470ohm | C25117 | R_0402_1005Metric |
| 2 | R9, R10 | 5.1kohm | C25905 | R_0402_1005Metric |
| 1 | R12 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R13 | 1kohm | C11702 | R_0402_1005Metric |
| 1 | RS1 | 15ohm | C22810 | R_0603_1608Metric |
| 12 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10, SW12, SW13 | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW11 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 1 | U1 | ESP32-S3-WROOM-1-N16R8 | C2913202 | ESP32-S3-WROOM-1_module_18x25.5mm_40_edge_pads_EPAD |
| 1 | U2 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U3 | MCP23017-E/SO | C47023 | SOIC-28W_7.5x17.9mm_P1.27mm |
| 1 | U4 | PCM5102APWR | C107671 | TSSOP-20_4.4x6.5mm_P0.65mm |
| 1 | U5 | PAM8403DR | C17337 | SOIC-16_3.9x9.9mm_P1.27mm |
| 1 | U6 | TLV75533PDBVR | C404027 | SOT-23-5 |
| 1 | U7 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U8 | MT3608 | C84817 | SOT-23-6 |
| 1 | U9 | ESD 2-line | C7519 | SOT-23-6 |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

Everything of variant 08 (Xtensa LX7 cores, interrupt matrix, MSPI/cache/MMU, octal PSRAM, GDMA,
boot ROM, GPIO matrix), plus:
- **LCD_CAM in RGB mode** as a timed DMA bus master: PCLK, porches and sync widths from its registers,
  reading PSRAM through the cache/EDMA at the pixel rate — a frame produced by timing, not by commands;
  underruns when the firmware starves it.
- The **raw panel** as a board component: a model that samples the 16 data lines at DE/PCLK and
  assembles frames (or, as a fast path, the SoC hands the engine the frame buffer address and timing).
- **I2C GPIO expander** (MCP23017 register file, interrupt-on-change, open-drain INT) on the link.
- **Constant-current boost** driving an LED string from PWM (brightness = duty), the charger's power
  path and two 18650 cells as a discharge curve, stereo audio out.

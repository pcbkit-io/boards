# 09 · s3-rgb — a 4.3" RGB-panel Doom on the ESP32-S3's LCD_CAM

The big-screen variant: the ESP32-S3 scans a **raw RGB TFT** (no controller, no frame memory) out of
a frame buffer in its octal PSRAM with the LCD_CAM peripheral and DMA — the architecture of the
popular ESP32-S3 "4.3-inch" boards. Ten buttons on an I2C expander (the pins all go to the panel),
stereo sound through a DAC and an amplifier, and a power-bank SoC for the battery.

| | |
|---|---|
| Processor | ESP32-S3-WROOM-1-N16R8 (2 × Xtensa LX7 240 MHz, 512 KB SRAM, 16 MB flash, 8 MB octal PSRAM) |
| Display | Newhaven **NHD-4.3-480272EF-ASXN**: 4.3" 480 × 272, 24-bit RGB parallel, DE mode, DCLK 9–15 MHz; driven as RGB565 (R3–R7 IO1/2/4/5/6, G2–G7 IO7/15/16/17/18/8, B3–B7 IO9–IO13; PCLK IO42, DE IO40, VSYNC IO41, HSYNC IO39; the low bits grounded) through a 40-pin 0.5 mm FFC connector (Molex 54132-4062) on the back |
| Backlight | 12 white LEDs, 25.6 V at 40 mA: **MT3608 boost as a constant-current driver** (FB regulates 0.6 V over 15 Ω), EN/PWM on IO46 (a strap that must be low at reset: 100 kΩ pull-down) |
| Input | 10 tact switches on an **MCP23017** (I2C 0x20, IO14 SDA, IO38 SCL), interrupt on change to IO3 |
| Sound | I2S (IO47 BCK, IO48 LRCK, IO21 DIN) → **PCM5102A** DAC → **PAM8403** stereo class-D → two 4–8 Ω speakers |
| Power | USB-C VBUS → **IP5306** power-bank SoC (2.1 A switching charger, power path, 5.1 V boost from the cell, its 4-LED gauge, its power key) → V5 for the backlight and the amplifier → **AMS1117-3.3** for the logic |
| Programming | USB-C D+/D− to the S3's USB (IO19/IO20): esptool / USB Serial/JTAG |
| Board | 182 × 80 mm, **4 layers**; front: the panel on tape (outline in the silkscreen), buttons, the power corner (IP5306, key, gauge LEDs), USB-C; back: module (antenna past the top edge), FFC connector, backlight driver, expander, audio |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 2 warnings, 0 unrouted nets; 85 components, 76 nets, 792 track segments (2617 mm), 99 vias.

| Warning | Why it is acceptable |
|---|---|
| `diff-pair-uncoupled`: pair DP / DN: 109.03 mm uncoupled (0 mm coupled), over the pair's default's 10 mm | USB full speed (12 Mbit/s): the connector is on the front edge, the processor on the back; tens of mm uncoupled are harmless at this rate |
| `skew`: DN: 109.34 mm, 5.75 mm shorter than DP (115.09 mm), over the pair's default's 0.1 mm skew | USB full speed: millimetres of skew are picoseconds against 4–20 ns edges |

<!-- /STATUS -->

## Why it runs Doom (and what the screen costs)

Doom is the same port as variant 08. The difference is the display: the LCD_CAM peripheral in RGB
mode streams the frame buffer from PSRAM by GDMA continuously — 480 × 272 × 2 B = 261 KB per frame,
about 15.7 MB/s at 60 Hz, a third of the octal PSRAM's ~40–50 MB/s usable bandwidth (estimate;
Espressif's RGB-panel guides use "bounce buffers" in SRAM to protect it). Doom renders 320 × 200 in
SRAM, the second core scales it 1.36× (or integer 1× centred) into the PSRAM frame buffer. The
panel never stops refreshing, which is why this design matters to the simulator: the display is a
**timed bus master**, not a sequence of SPI commands.

## Power

| State | Current (estimate) |
|---|---|
| ESP32-S3 + PSRAM streaming the panel | ~120 mA at 3.3 V |
| Panel logic | 25 mA at 3.3 V |
| Backlight 25.6 V × 40 mA ≈ 1.0 W | ~230 mA from 5.1 V at 85 % |
| PAM8403, moderate volume | ~100 mA from 5.1 V |
| **Typical** | **≈ 2.3 W from the cell → ~650 mA at 3.7 V** |

A 3000 mAh LiPo → **~4.5 h**. The AMS1117 burns (5.1 − 3.3) × 0.15 A ≈ 0.27 W: fine for SOT-223, the
price of a simple design (a buck there would add ~40 min). The IP5306 switches off below ~45 mA load
after 32 s — never reached while the panel is lit; its key turns the boost on and off.

## Firmware: build and flash

- ESP-IDF 5.x with `esp_lcd_panel_rgb` (timings from the Newhaven sheet: HSYNC back porch 43, front
  porch 2, width 1; VSYNC back porch 12, front porch 1–4; 9 MHz PCLK), bounce buffers in SRAM, the
  frame buffer in PSRAM; `esp_lcd` I2S/PCM driver; MCP23017 over the I2C master driver.
- Flash: `idf.py flash` over USB-C; recovery as variant 08 (BOOT is the module's IO0 button on the
  back, from its support circuit).

## Bill of materials

Rough cost at 10 pieces (estimates): ESP32-S3-WROOM-1-N16R8 $4.50 · NHD-4.3-480272EF-ASXN $35 (a
generic 4.3" 480 × 272 40-pin panel: $8) · Molex 54132-4062 $1.00 · MT3608 $0.15 · MCP23017 $1.20 ·
PCM5102A $2.00 · PAM8403 $0.15 · IP5306 $0.50 · AMS1117 $0.10 · USB-C $0.40 · buttons $0.50 ·
passives $1.80 · 4-layer PCB $6.00 · 2 speakers $2.00 · 3000 mAh LiPo $7.00 → **about $63** with the
Newhaven panel, ~$36 with a generic one.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 1 | C1 | 10uF |  | C_0603_1608Metric |
| 1 | C2 | 22uF |  | C_0805_2012Metric |
| 1 | C3 | 4.7uF |  | C_1206_3216Metric |
| 7 | C4, C22, C28, C30, C31, C32, C34 | 22uF |  | C_1206_3216Metric |
| 6 | C5, C7, C8, C10, C12, C16 | 100nF |  | C_0402_1005Metric |
| 5 | C6, C19, C20, C21, C23 | 1uF |  | C_0402_1005Metric |
| 7 | C9, C11, C13, C26, C27, C29, C33 | 10uF |  | C_0805_2012Metric |
| 2 | C14, C15 | 2.2uF |  | C_0805_2012Metric |
| 2 | C17, C18 | 2.2nF |  | C_0402_1005Metric |
| 2 | C24, C25 | 470nF |  | C_0402_1005Metric |
| 1 | D1 | schottky | B5819W | D_SOD-123 |
| 4 | D2, D3, D4, D5 | blue |  | LED_0603_1608Metric |
| 1 | D6 | tvs 5V |  | D_SOD-323 |
| 1 | J1 |  | 54132-4062 | Conn_FFC-FPC_1x40_P0.50mm_Horizontal |
| 3 | J2, J5, J6 |  | S2B-PH-K-S(LF)(SN) | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | J3 | USB_C_Receptacle_USB2.0_16P |  | USB_C_Receptacle_USB2.0_16P |
| 1 | L1 | 22uH |  | L_Power_4x4mm |
| 1 | L2 | 1uH |  | L_Power_6x6mm |
| 3 | R1, R3, R4 | 10kohm |  | R_0402_1005Metric |
| 1 | R2 | 100kohm |  | R_0402_1005Metric |
| 2 | R5, R6 | 4.7kohm |  | R_0402_1005Metric |
| 2 | R7, R8 | 470ohm |  | R_0402_1005Metric |
| 2 | R9, R10 | 5.1kohm |  | R_0402_1005Metric |
| 2 | R11, R12 | 2ohm |  | R_0402_1005Metric |
| 1 | RS1 | 15ohm |  | R_0603_1608Metric |
| 13 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10, SW11, SW12, SW13 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | U1 |  | ESP32-S3-WROOM-1-N16R8 | ESP32-S3-WROOM-1_module_18x25.5mm_40_edge_pads_EPAD |
| 1 | U2 |  | IP5306 | SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.09x2.09mm |
| 1 | U3 |  | MCP23017-E/SO | SOIC-28W_7.5x17.9mm_P1.27mm |
| 1 | U4 |  | PCM5102APWR | TSSOP-20_4.4x6.5mm_P0.65mm |
| 1 | U5 |  | PAM8403DR | SOIC-16_3.9x9.9mm_P1.27mm |
| 1 | U6 |  | AMS1117-3.3 | SOT-223 |
| 1 | U7 | ESD 2-line |  | SOT-23-6 |
| 1 | U8 |  | MT3608 | SOT-23-6 |
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
- **Constant-current boost** driving an LED string from PWM (brightness = duty), **IP5306** as a
  power-bank state machine (key presses, auto-off at light load, gauge LEDs), stereo audio out.

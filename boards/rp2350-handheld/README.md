# RP2350 Doom handheld

A carrier board for a **Pimoroni Pico Plus 2** (RP2350B with 16 MB of flash and 8 MB of PSRAM)
under a **2.8" ILI9341 SPI TFT module** (the MSP2807, with touch), with a d-pad, strafe, fire, use,
menu and enter buttons. Power and flashing go through the Pico's USB-C. The outline is a red
keycard with a lanyard slot; the wordmark on the silkscreen is our own drawing.

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/doom-handheld/tree/main/boards/rp2350-handheld, open
the Laboratory and press **Run**: the page builds `doom/` for the RP2350 (with `DOOM1.WAD` linked
into flash), boots the image on a register-level model of the chip and its PSRAM, and draws the
ILI9341's frames on the board. Play with the keyboard (arrows, F fire, Space use, comma/period
strafe, M menu, Enter) or a gamepad (`input.js` maps a standard controller).

## Parts

| Ref | Part | Note |
|---|---|---|
| U1 | Pimoroni Pico Plus 2 (PIM722) | on the back, on two 1x20 headers |
| lcd | 2.8" ILI9341 SPI TFT with touch (MSP2807) | on a 1x14 female header; the touch shares SPI0 and stays deselected |
| up, down, left, right, strafeLeft, strafeRight, fire, use, menu, enter | 6 x 6 mm tactile switch | to ground; the RP2350's pull-ups |
| J1 | 1x3 2.54 mm header | the console: UART0 at 115200 baud (TX, RX, GND) |

## Pins

| Signal | GPIO |
|---|---|
| Display SCK, MOSI (SPI0) | GP18, GP19 |
| Display CS, D/C, RESET, backlight | GP17, GP20, GP21, GP22 |
| Up, down, left, right, fire, use, strafe left, strafe right, menu, enter | GP2 … GP11 |
| Console TX, RX (UART0) | GP0, GP1 |
| PSRAM chip select | GP47 (on the Pico Plus 2) |

## The firmware

`doom/doom_rp2350.c` is the platform layer: clocks (12 MHz crystal, 150 MHz from the PLL), the
PSRAM on the QSPI interface, the display over SPI0 at 25 MHz, the buttons, the timer. The engine
in `doom/` is sysprog21/doom_riscv, unchanged. The program runs on the RP2350's RISC-V (Hazard3)
cores from flash; the zone heap and the screens are in PSRAM. There is no sound.

The simulation runs one instruction per clock and does not charge flash or PSRAM latency, so the
board itself may draw fewer frames per second than the browser shows.

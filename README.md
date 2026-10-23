# Doom handhelds

Circuit boards that run Doom, designed in [PCBKit](https://pcbkit.io). Every board is a PCBKit
project (a KiCad project with its `.pcbkit/` design): open one in the app, press **Run**, and the
same firmware that would be flashed onto the ordered board is built in your browser and plays
there on a simulation of its chips; order the board, flash the image, and it plays on the desk.

Open them at **https://app.pcbkit.io/github/pcbkit-io/doom-handheld**.

| Board | What it is |
|---|---|
| [`boards/rp2350-handheld`](boards/rp2350-handheld) | Pimoroni Pico Plus 2 (RP2350B, 16 MB flash, 8 MB PSRAM) under a 2.8" ILI9341 screen, ten buttons, USB-C power; a red keycard with a lanyard slot |

Ten more designs explore the range, from the cheapest chip that can run Doom at all to a
Cortex-M7 at 600 MHz: different processors, screens, memories and sound, one power design. Each
is routed, passes `check` and is ready to order; their programs are not written yet, and only the RP2040
and RP2350 ones can run in PCBKit's simulation today (each README says what the others need).

| Board | Processor | Screen | Flashing on arrival |
|---|---|---|---|
| [`boards/v003-emu`](boards/v003-emu) | CH32V003 (RISC-V, 2 KB RAM) emulating a larger RISC-V, game in 8 MB SPI PSRAM | Nokia 5110 | WCH-LinkE on the SWIO header |
| [`boards/pico-oled`](boards/pico-oled) | Raspberry Pi Pico (RP2040) on sockets | 2.42" mono OLED | the Pico's USB: BOOTSEL, UF2 |
| [`boards/c3-pocket`](boards/c3-pocket) | ESP32-C3 module (RISC-V) | 1.3" colour TFT | USB-C: the C3's USB Serial/JTAG (esptool) |
| [`boards/rp2040-tft`](boards/rp2040-tft) | bare RP2040, 16 MB flash, microSD, PWM sound | 1.8" colour TFT | USB-C: BOOT + RESET, UF2; SWD header |
| [`boards/f411-psram`](boards/f411-psram) | STM32F411 (Cortex-M4F) with 8 MB SPI PSRAM and flash, I2S amplifier | 2.0" TFT | USB-C: the ROM's DFU (BOOT + RESET); SWD header |
| [`boards/pico2-carrier`](boards/pico2-carrier) | Pimoroni Pico Plus 2 carrier with a headphone DAC | 2.8" ILI9341 touch | the module's USB: BOOTSEL, UF2 |
| [`boards/rp2350-psram`](boards/rp2350-psram) | bare RP2350A with QSPI flash and 8 MB PSRAM, microSD, I2S amplifier, fuel gauge | 2.0" TFT | USB-C: BOOT + RESET, UF2; SWD header |
| [`boards/s3-handheld`](boards/s3-handheld) | ESP32-S3 module (Xtensa, 8 MB PSRAM), microSD, I2S amplifier, fuel gauge | 2.8" touch TFT | USB-C: the S3's USB Serial/JTAG (esptool) |
| [`boards/s3-rgb`](boards/s3-rgb) | ESP32-S3 driving a raw 480×272 RGB panel, buttons on an I2C expander, stereo DAC | 4.3" RGB | USB-C: the S3's USB Serial/JTAG (esptool) |
| [`boards/teensy41`](boards/teensy41) | Teensy 4.1 (Cortex-M7, 600 MHz), DAC with jack and speaker, fuel gauge | 3.5" touch TFT | the Teensy's USB: Teensy Loader |

Every board runs from **two 18650 Li-ion cells in parallel** (1S2P, Keystone 1042 holders on the
back) behind a reverse-cell P-MOSFET and DW01A + FS8205A protection, charged over USB by a BQ24075
with power path (the board runs from USB while it charges), as [`boards/rp2350-handheld`](boards/rp2350-handheld)
does. Every assembled part has an LCSC code for JLCPCB's assembly; modules and screens plug into
sockets it solders. Each board's README says how to charge it, how to flash it, what to plug in
by hand and its parts, with its `check` and readiness results.

## Licences

- The game engine in each board's `doom/` is [sysprog21/doom_riscv](https://github.com/sysprog21/doom_riscv)
  at `81ccabe`, unchanged: id Software's Linux Doom source as released under the GNU GPL v2.
- Everything else here (the boards, their designs and KiCad files, the platform layers
  `doom/doom_*.c`, the simulations' scripts) is by PCBKit, under the same licence: the GNU
  General Public License v2 ([`LICENSE`](LICENSE)).
- The game's data is not in this repository. Each design declares the shareware `DOOM1.WAD`
  (id Software, free to share unmodified) by where it is published and its SHA-256; the build
  downloads it, checks it and links it into the firmware.

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
Cortex-M7 at 600 MHz: different processors, screens, memories, sound and batteries with their
chargers. Each routes and passes `check`; their programs are not written yet, and only the RP2040
and RP2350 ones can run in PCBKit's simulation today (each README says what the others need).

| Board | Processor | Screen | Power |
|---|---|---|---|
| [`boards/v003-emu`](boards/v003-emu) | CH32V003 (RISC-V, 2 KB RAM) emulating a larger RISC-V, game in 8 MB SPI PSRAM | Nokia 5110 | 2×AA, boost to 3.3 V |
| [`boards/pico-oled`](boards/pico-oled) | Raspberry Pi Pico (RP2040), through-hole kit | 2.42" mono OLED | 3×AA |
| [`boards/c3-pocket`](boards/c3-pocket) | ESP32-C3 module (RISC-V) | 1.3" colour TFT | LiPo, charger, protection |
| [`boards/rp2040-tft`](boards/rp2040-tft) | bare RP2040, 16 MB flash, microSD, PWM sound | 1.8" colour TFT | LiPo, charger, protection |
| [`boards/f411-psram`](boards/f411-psram) | STM32F411 (Cortex-M4F) with 8 MB SPI PSRAM and flash, I2S amplifier | 2.0" TFT | one 18650 |
| [`boards/pico2-carrier`](boards/pico2-carrier) | Pico 2 or Pico Plus 2 carrier with a headphone DAC | 2.8" ILI9341 touch | USB |
| [`boards/rp2350-psram`](boards/rp2350-psram) | bare RP2350A with QSPI flash and 8 MB PSRAM, microSD, I2S amplifier | 2.0" TFT | LiPo, charger with power path, fuel gauge |
| [`boards/s3-handheld`](boards/s3-handheld) | ESP32-S3 module (Xtensa, 8 MB PSRAM), microSD, I2S amplifier | 2.8" touch TFT | LiPo, charger, fuel gauge |
| [`boards/s3-rgb`](boards/s3-rgb) | ESP32-S3 driving a raw 480×272 RGB panel, buttons on an I2C expander, stereo DAC | 4.3" RGB | IP5306 power bank chip |
| [`boards/teensy41`](boards/teensy41) | Teensy 4.1 (Cortex-M7, 600 MHz), DAC with jack and speaker | 3.5" touch TFT | LiPo, charger, fuel gauge, 5 V boost |

## Licences

- The game engine in each board's `doom/` is [sysprog21/doom_riscv](https://github.com/sysprog21/doom_riscv)
  at `81ccabe`, unchanged: id Software's Linux Doom source as released under the GNU GPL v2.
- Everything else here (the boards, their designs and KiCad files, the platform layers
  `doom/doom_*.c`, the simulations' scripts) is by PCBKit, under the same licence: the GNU
  General Public License v2 ([`LICENSE`](LICENSE)).
- The game's data is not in this repository. Each design declares the shareware `DOOM1.WAD`
  (id Software, free to share unmodified) by where it is published and its SHA-256; the build
  downloads it, checks it and links it into the firmware.

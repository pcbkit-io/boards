# Doom handhelds

Circuit boards that run Doom, designed in [PCBKit](https://pcbkit.io). Every board is a PCBKit
project (a KiCad project with its `.pcbkit/` design): open one in the app, press **Run**, and the
same firmware that would be flashed onto the ordered board is built in your browser and plays
there on a simulation of its chips; order the board, flash the image, and it plays on the desk.

Open them at **https://app.pcbkit.io/github/seralexeev/doom-handheld**.

| Board | What it is |
|---|---|
| [`boards/rp2350-handheld`](boards/rp2350-handheld) | Pimoroni Pico Plus 2 (RP2350B, 16 MB flash, 8 MB PSRAM) under a 2.8" ILI9341 screen, ten buttons, USB-C power; a red keycard with a lanyard slot |

## Licences

- The game engine in each board's `doom/` is [sysprog21/doom_riscv](https://github.com/sysprog21/doom_riscv)
  at `81ccabe`, unchanged: id Software's Linux Doom source as released under the GNU GPL v2.
- Everything else here (the boards, their designs and KiCad files, the platform layers
  `doom/doom_*.c`, the simulations' scripts) is by PCBKit, under the same licence: the GNU
  General Public License v2 ([`LICENSE`](LICENSE)).
- The game's data is not in this repository. Each design declares the shareware `DOOM1.WAD`
  (id Software, free to share unmodified) by where it is published and its SHA-256; the build
  downloads it, checks it and links it into the firmware.

# PCBKit boards

Circuit boards as plain KiCad projects, each one you can open in [PCBKit](https://pcbkit.io),
run as the device it is, and order. The KiCad files are the design; a board with a processor carries its program beside it
(`firmware/`, or the folder its `.pcbkit/device.yaml` names) and PCBKit builds it and runs the
image on its models of the board's chips, wired as the board's copper wires them: the same image
you flash onto the ordered board.

What a board cannot say itself is in its `.pcbkit/device.yaml`: the program's folder where it is
not `firmware`, files the program is built with and the repository does not hold (the Doom
boards' `DOOM1.WAD`, by address and SHA-256), a bare panel's size, the keys that work its
buttons. `pcbkit device plan` in a board's folder says what was recognised and how it is wired;
`pcbkit device build` and `pcbkit device run --seconds 5` build and run it.

| Folder | What is in it |
|---|---|
| [`devices/`](devices) | Real devices end to end: an STM32G431 sensor node on a robot's CAN bus and a 24 V AC thermostat on an ESP32, each with the world around it simulated |
| [`lab/`](lab) | Interactive boards: an ATmega328P (or two) running real C firmware against sensors, buttons, knobs, displays and servos you can change while it runs |
| [`arduino/`](arduino) | Arduino projects to play with: an Arduino Nano running a sketch with the usual libraries (Adafruit SSD1306 and SHT31, Sensirion SCD4x, LiquidCrystal, Servo): a weather station, a CO₂ monitor, a thermostat, a character-LCD runner, a pocket console with six games, Simon, a drawing screen |
| [`examples/`](examples) | Boards to learn from: a design PCBKit placed and routed from parts and connections, circuit blocks, open boards by their authors |
| [`assets/`](assets) | Files the simulation needs besides the boards: the boot ROMs of Espressif chips (Apache 2.0, from Espressif) |
| [`doom/`](doom) | Doom handhelds: eleven boards from a 10-cent CH32V003 to a Teensy 4.1 (RP2040, RP2350, ESP32-C3, ESP32-S3, STM32F411, i.MX RT1062), each ordered at JLCPCB as it is, flashed over USB and playing Doom from two 18650 cells |

Each folder is a family of boards with its own README; each board's folder is one project with
its README (what it is, how to order, flash and charge it, its parts and check results).

## The registry

[`registry.json`](registry.json) is the catalogue of these boards: their categories and, for each
board, its title, summary, tags, chips, licence, author, origin, folder, how it opens and whether
it is ready to order (`production`), as the schema [`registry.schema.json`](registry.schema.json)
says. PCBKit's app, its board library and pcbkit.io read nothing else: `node scripts/publish.mjs`
checks every board with the pcbkit command-line tool and publishes the registry and each board's
files to https://library.pcbkit.io/demos/ (the app reads it there without being rebuilt); a
board's picture is PCBKit's, drawn from its folder at the published commit
(`https://app.pcbkit.io/embed/github/pcbkit-io/boards/tree/<commit>/<path>.webp`). A new board: its
folder and an entry, then publish.

**Every interactive board is a real board**: it runs in the app, and it can be ordered from a
fab as it is and flashed after assembly (a USB bootloader or a programming header), so the image
that runs in the app runs on the board.

## Checking a board

`pcbkit check` in a board's folder runs KiCad's own electrical and design rule checks on it and
says what they found (the command comes with [PCBKit's plugin](https://github.com/pcbkit-io/agent-plugin)).

## Licences

Each family has its own: `devices/`, `lab/`, `arduino/` and `examples/` are CC0 1.0 (public domain), but for the open
boards by other authors, which keep theirs (`LICENSE.md` in their folders); `doom/` is the GNU GPL
v2 ([`doom/LICENSE`](doom/LICENSE)), the licence of the Doom engines it carries.

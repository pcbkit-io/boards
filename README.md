# PCBKit boards

Circuit boards designed in [PCBKit](https://pcbkit.io), each a complete project you can open, run
and order. Every board is a PCBKit project (a KiCad project with its `.pcbkit/` design); a board
with a processor carries its program, which PCBKit builds in your browser and runs on a
simulation of its chips: the same image you flash onto the ordered board.

Open them all at **https://app.pcbkit.io/github/pcbkit-io/boards**, or one folder at a time.

| Folder | What is in it |
|---|---|
| [`lab/`](lab) | Interactive boards: an ATmega328P (or two) running real C firmware against sensors, buttons, knobs, displays and servos you can change while it runs |
| [`arduino/`](arduino) | Arduino projects to play with: an Arduino Nano running a sketch with the usual libraries (Adafruit SSD1306 and SHT31, Sensirion SCD4x, LiquidCrystal, Servo): a weather station, a CO₂ monitor, a thermostat, a character-LCD runner, a pocket console with six games, Simon, a drawing screen |
| [`examples/`](examples) | Boards to learn from: a design PCBKit placed and routed from parts and connections, circuit blocks, open boards by their authors |
| [`doom/`](doom) | Doom handhelds: eleven boards from a 10-cent CH32V003 to a Teensy 4.1 (RP2040, RP2350, ESP32-C3, ESP32-S3, STM32F411, i.MX RT1062), each ordered at JLCPCB as it is, flashed over USB and playing Doom from two 18650 cells |

Each folder is a family of boards with its own README; each board's folder is one project with
its README (what it is, how to order, flash and charge it, its parts and check results).

## Licences

Each family has its own: `lab/`, `arduino/` and `examples/` are CC0 1.0 (public domain), but for the open
boards by other authors, which keep theirs (`LICENSE.md` in their folders); `doom/` is the GNU GPL
v2 ([`doom/LICENSE`](doom/LICENSE)), the licence of the Doom engines it carries.

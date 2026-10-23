# Arduino boards

Arduino projects to play with: an Arduino Nano on each board, running a sketch written as anyone
writes one, with the libraries everyone uses (Adafruit's SSD1306 and GFX, Adafruit's SHT31,
Sensirion's SCD4x, LiquidCrystal, Servo, Wire, EEPROM). Open one and it runs: PCBKit builds the sketch in your
browser with avr-gcc and the Arduino core (the real ones, from
[pcbkit-io/toolchain](https://github.com/pcbkit-io/toolchain)) and runs it on a simulation of the
ATmega328P and of every part on the board. Press the keys, turn the knobs, change the weather;
edit the sketch and press **Build & run**. The same sketch builds in the Arduino IDE for the Nano
you plug into the ordered board.

Open them at **https://app.pcbkit.io/?category=arduino**, or one by its folder:
`https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/arduino/<board>`.

| Board | What it is | |
|---|---|---|
| [`weather-station`](weather-station) | Weather station | An Arduino Nano reads an SHT31 and shows the temperature and humidity large on an OLED, with the dew point and how the air feels, the day's lowest and highest and a graph of the last two minutes. Change the weather in the lab; the button turns the pages. |
| [`co2-monitor`](co2-monitor) | CO₂ monitor | Sensirion's SCD40 measures CO₂, temperature and humidity every five seconds; the OLED shows the concentration with a ten-minute graph and a traffic light says when to open a window. Raise the CO₂ in the lab, as a full room does, and watch it turn red. |
| [`thermostat`](thermostat) | Thermostat | A room thermostat that turns a radiator valve: a PI controller opens a servo as far as an SHT31 finds the room below the setpoint, the red light shows heating, − and + set the target and the EEPROM keeps it. Cool the room in the lab and watch the valve open. |
| [`lcd-runner`](lcd-runner) | LCD runner | The classic 16 × 2 character LCD on LiquidCrystal's wiring: a dinosaur made of custom characters runs along the bottom line, jumps cacti with the button and runs under birds; turn the trimmer and the contrast changes as on the real module. |
| [`snake`](snake) | Snake | Snake on the pocket console, an Arduino Nano behind a 0.96" OLED with a D-pad and two buttons: steer with the arrow keys, eat, grow and go faster; the best score stays in the EEPROM. |
| [`pong`](pong) | Pong | Pong on the pocket console: you on the left with up and down, the computer on the right; the ball speeds up with every return and leaves the paddle at an angle. First to seven. |
| [`flappy`](flappy) | Flappy bird | A flappy bird on the pocket console: X flaps, gravity does the rest, pipes scroll in with a gap to fly through; one point a pipe and the best kept in the EEPROM. |
| [`breakout`](breakout) | Breakout | Breakout on the pocket console: move the paddle with left and right, serve with X; five rows of bricks, three balls, the angle off the paddle set by where the ball lands. |
| [`tetris`](tetris) | Falling blocks | Falling blocks on the pocket console: a 10 × 20 well beside the next piece and the score; left and right move, X turns, down drops faster, Z drops at once; full lines clear and the pace rises. |
| [`dice`](dice) | Dice | Dice on the pocket console: X rolls one to five dice that tumble before they settle, up and down choose how many, Z shows how often each sum came up. |
| [`simon`](simon) | Simon | Simon on a round board: four coloured lights, each with its button; the Nano plays a sequence and you repeat it, one more every round, a little faster. Keys 1 to 4 press the buttons; the best round stays in the EEPROM. |
| [`etch-a-sketch`](etch-a-sketch) | Magic drawing screen | Two knobs draw on an OLED, one across and one up and down, as on the toy; the button wipes the picture. Turn the knobs in the lab. |

The pocket console (`snake`, `pong`, `flappy`, `breakout`, `tetris`, `dice`) is one board with
a game each: a 0.96" I2C OLED in front, a D-pad and two buttons, the Nano on the back. In the lab
the arrow keys are the D-pad, X is A and Z is B.

Each folder is a PCBKit project: `.pcbkit/design.pcbkit` (what the board is), the KiCad board
the tools wrote, `firmware/` (the sketch and its built image, `image.json`) and `SOURCE.md`.

## Licence

These boards and their sketches are dedicated to the public domain:
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) (SPDX `CC0-1.0`). The libraries they
use keep their own licences (LGPL-2.1 for the Arduino core and Servo, BSD for Adafruit's and
Sensirion's); an image built from a sketch carries them.

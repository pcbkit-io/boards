# Interactive boards

Boards with a processor and the program it runs, made to be tried: open one in PCBKit and it
starts at once, its firmware running on a simulation of its chips, its parts on the board answering
(buttons, knobs, sensors, displays, servos, lights). Change the conditions (temperature, humidity,
CO₂, a button held), edit the program and press **Build & run**: the same image is what you flash
onto the ordered board.

Open them at **https://app.pcbkit.io/?category=lab**, or one by its folder:
`https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/lab/<board>`.

| Board | What it is | |
|---|---|---|
| [`air-quality`](air-quality) | Air quality meter | A battery-powered air meter you can run: an ATmega328P with real C firmware reads an SHT31 and an SCD40 and shows temperature and humidity on a MAX7219 display, with green, orange and red CO₂ lights. Change the weather and watch the firmware answer. |
| [`servo-controller`](servo-controller) | Servo controller | A device you can run: an ATmega328P reads a knob and a button and drives a servo with PWM, sleeping between interrupts. Turn the knob or press the button and watch the servo follow. |
| [`serial-device`](serial-device) | Serial command device | A device you talk to over UART: an ATmega168P echoes every byte and turns its light on for 1 and off for 0. Type into its serial console and watch it answer. |
| [`led-ring-lab`](led-ring-lab) | LED ring | Twelve lights round a black disc, each dimmed by software PWM from a timer interrupt: a spinning comet with a fading tail, a dial gauge of the knob from green to red, a breathing wave. The button changes the mode, the knob the speed. |
| [`stopwatch`](stopwatch) | Stopwatch | A watch face that keeps time: a 1 kHz timer interrupt counts in decimal digits and a MAX7219 shows hours to hundredths. Flip RUN to start and stop it; LAP freezes the display while it keeps counting, or resets it when stopped. |
| [`reaction-game`](reaction-game) | Reaction game | How fast are you? Press to start: red, amber, a random wait, then green and GO. Press again and the display shows your time in milliseconds and your best; pressing before green is a false start. |
| [`greenhouse`](greenhouse) | Greenhouse controller | A house-shaped board that opens its window: an SHT31 reads the temperature, an SCD40 the CO₂, and a servo opens the window when it is too warm or stuffy. Turn the setpoint knob, change the temperature and CO₂ in the lab, and watch it react. |
| [`servo-sequencer`](servo-sequencer) | Servo sequencer | Three servos from one timer interrupt. They dance a built-in choreography until you press TEACH: move them with the three knobs, press again and they replay your motion in a smooth loop. |
| [`pedestrian-crossing`](pedestrian-crossing) | Pedestrian crossing | A traffic light for cars and one for people, as a state machine: press PUSH and the countdown starts, cars get amber and red, WALK shows the seconds left and flashes before it ends. |
| [`message-scroller`](message-scroller) | Message scroller | Type into the serial console and your words scroll across eight seven-segment digits, letters included, in a font of the program's own. A UART receive interrupt collects the line; the knob sets the speed. |

Each folder is a PCBKit project: `.pcbkit/design.pcbkit` (what the board is), the KiCad board
the tools wrote, `firmware/` (the C program and its built image, `image.json`) and `SOURCE.md`.

## Licence

Our own boards here are dedicated to the public domain: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)
(SPDX `CC0-1.0`); use, change and share them without conditions. A board that began elsewhere keeps
its original's licence in its own `LICENSE.md`, with its author and source in `SOURCE.md`.

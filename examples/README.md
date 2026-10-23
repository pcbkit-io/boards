# Example boards

Boards to learn PCBKit from: a design written as parts and connections that the tools placed and
routed, circuit blocks of the standard library at work, and open boards by their authors
converted to PCBKit. Open one at **https://app.pcbkit.io/demo/<board>** or from its folder.

| Board | What it is | |
|---|---|---|
| [`blinker`](blinker) | 555 blinker | A 555 timer flashing an LED, written as parts and connections only: the engine places the parts, sizes the board and routes it. |
| [`led-bar`](led-bar) | LED bar | Eight LED channels from one block on a shift register: a net class for the supply, a ground pour, a placement rule, black mask and gold. |
| [`usb-uart`](usb-uart) | USB-C serial adapter | A USB-C to UART adapter composed from the standard circuit blocks: the Type-C port with its CC resistors and ESD array, a 3.3 V regulator, the USB to UART bridge, a power LED and the header. The blocks sized and typed every part; the engine placed them. |
| [`usb-dac`](usb-dac) | USB audio DAC | A USB sound card: a USB audio chip with its crystal and regulator, on four layers with ground pours. |
| [`robot-controller`](robot-controller) | Robot controller | The main board of a small autonomous robot from a student robotics club: a wireless microcontroller module, motor driver and regulators, 70 parts on four layers. |

## Licence

Our own boards here are dedicated to the public domain: [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)
(SPDX `CC0-1.0`); use, change and share them without conditions. A board that began elsewhere keeps
its original's licence in its own `LICENSE.md`, with its author and source in `SOURCE.md`.

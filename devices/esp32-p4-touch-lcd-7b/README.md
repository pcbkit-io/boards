# esp32-p4-touch-lcd-7b — Waveshare's ESP32-P4 7" touch board, running PocketJS

Waveshare's [ESP32-P4-WIFI6-Touch-LCD-7B](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B)
as a PCBKit project: its circuit read from Waveshare's published schematic, its parts where
Waveshare's assembly drawing puts them (the connectors on the same edges, the chips in the same
places), behind the same 7" 1024 × 600 panel. In the app it runs
[PocketJS](https://github.com/pocket-nexus/pocketjs)'s ESP32-P4 host for this board (pull
request #208, MIT) with its stock Feature Cards app: one flash image, published by PCBKit as a
[release of this repository](https://github.com/pcbkit-io/boards/releases/tag/pocketjs-esp32p4-7b-5d4a16b9),
booted by Espressif's ESP32-P4 boot ROM on PCBKit's model of the chip.

| | |
|---|---|
| Processor | ESP32-P4NRW32 (two RISC-V cores at 360 MHz, 32 MB PSRAM in package), GD25Q256 32 MB flash, 40 MHz and 32.768 kHz crystals, a CR1220 for its clock |
| Screen | 7" 1024 × 600 IPS, EK79007 over 2-lane MIPI-DSI (P2, 30 pins), GT911 touch on I2C (J3, 6 pins, GPIO7/8, INT GPIO23); AVDD 9.6 V from an AP3012, VGH and VGL from charge pumps, the backlight from an AP3032 dimmed by GPIO32 |
| Sound | ES8311 codec, NS4150B 3 W amplifier (GPIO53) to the speaker (H7); ES7210 four-channel ADC with two ZTS6117 microphones and the speaker's own signal for echo cancelling; I2S on GPIO9–13 |
| Radio | ESP32-C6-MINI-1 on the P4's second SD/MMC slot (GPIO14–19), its antenna at the top edge |
| Storage | microSD on SD/MMC slot 0 (GPIO39–44), its supply switched by GPIO45 |
| Camera | 15-pin 1 mm MIPI-CSI connector (J2), Raspberry Pi's pinout |
| Ports | RS-485 (SP3485, automatic direction), CAN (TJA1051T/3), I2C through an NDC7002N level shifter, the console's UART, two 12-pin headers of GPIOs (PH2.0) |
| USB | USB-C console (CH343P, automatic boot), USB-C to the P4's full-speed USB, USB-A host (1 A switch) |
| Power | the two USB-C ORed by ideal diodes, a Li-ion cell (IP5306 charger and boost), a slide switch, SY8089 bucks to 3.3 V and to the P4's 1.2 V |
| Board | 165.5 × 98 mm, 6 layers (JLCPCB JLC06161H-3313), four M2.5 holes |

Parts Waveshare uses that JLCPCB did not stock (2026-10-25), each replaced by one doing the same
job: the MP1658 and MP1605 bucks by SY8089s, the DIO7003 load switch by an SY6280, the ETA6098
charger and SCT12A0 boost by an IP5306, the EMH4 dual transistor by two MMBT3904s, the THVD1406 by
an SP3485. The RS-485 gas discharge tube and the 120 Ω termination jumpers are left out.

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors,
47 warnings (the DSI, CSI and USB pairs' coupling and skew: the router lays each pair as two
tracks; 26 tracks narrowed to 0.09 mm between the P4's pins; one 3.3 V track's current), 0
unrouted nets. 293 components, 206 nets, 3993 track segments, 852 vias.
`analysis.run { lens: "ready" }`: 17 to fix — the FPC connectors (P2, J3, J2) inside the board, as
the panel's tails and the camera's ribbon reach them from behind; decoupling 3–6 mm from some P4
pins (its 0402s on the back) and 10–14 mm from the C6 and the IP5306; the analysis's estimate of
ground's heating (it gives each ground stub to the plane the board's whole current).
Every assembled line has its LCSC code (110, chosen by stock at JLCPCB, 2026-10-25); `pcbkit export
--format fab` succeeds without `--force`. **JLCPCB has no ESP32-P4 in stock**: send it as a
consigned part.
<!-- /STATUS -->

## Run it in the browser

Open the board and press **Run**: the page fetches the PocketJS image (checked against the
SHA-256 this design declares) and Espressif's ROM, and runs them. Feature Cards comes up in
about seven seconds; tap a card on the screen in 3D and it opens its details. The same release has
PocketJS's Music ("Now Playing") and Chrome apps for this host: put one of their URLs, SHA-256s
and sizes into the processor's `image` to run it instead. The console (H8) shows PocketJS's
log.

What does not run here, and says so:

- **No radio.** The ESP32-C6 is on the board but held in reset: its firmware (ESP-Hosted) does
  not run, so there is no Wi-Fi or Bluetooth. Nothing pretends to be a network.
- **CAN.** PCBKit's ESP32-P4 model has no TWAI controller yet: the TJA1051 stays unconnected
  (the laboratory lists it among its limitations).
- The ES7210 (the microphones), the camera and the RS-485 transceiver have no models yet.

## Order, flash

1. Order with assembly (`pcbkit export --format fab`); the panel plugs into P2 and J3. JLCPCB has
   no ESP32-P4 in stock: send it as a consigned part.
2. Flash over the console's USB-C: `esptool.py --chip esp32p4 write_flash 0
   pocketjs-esp32p4-waveshare-7b-cards-factory.bin` (the CH343P's DTR and RTS put the P4 into
   its boot loader).

Licence: the board CC0 1.0; Waveshare's schematic is its own, read for the circuit (no files of
it here); PocketJS MIT, fetched from the release and never copied into the project.

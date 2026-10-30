# p4-touch-7 — a 7" ESP32-P4 touch screen running NucleoOS

A board for [NucleoOS-P4](https://github.com/indecenti/NucleoOS-P4) (by indecenti): the
Guition JC1060P470C's circuit (its published schematic) at NucleoOS's own pin map, laid out by
PCBKit behind a 7" 1024 × 600 MIPI-DSI touch panel. In the app it runs NucleoOS's own release,
unmodified: the factory image from its GitHub release, booted by Espressif's ESP32-P4 boot ROM on
PCBKit's model of the chip, the card in the microSD socket holding the release's SD-card archive.

| | |
|---|---|
| Processor | ESP32-P4NRW32 (two RISC-V cores at 360 MHz, 32 MB PSRAM in package), W25Q128JV 16 MB flash, 40 MHz crystal |
| Screen | JC1060M070N-I: 7" 1024 × 600 IPS, JD9165 over 2-lane MIPI-DSI (FPC1, 30 pins), GT911 capacitive touch on I2C0 (FPC2, 6 pins); its AVDD from an MT3608, VGH and VGL from charge pumps, the backlight from a second MT3608 dimmed by LEDC (an 18 V zener into its FB holds the output near 19 V with the panel unplugged, under the 30 V its switch pin takes) |
| Radio | ESP32-C6-MINI-1 on the P4's second SD/MMC slot (ESP-Hosted), its antenna over the top edge |
| Audio | ES8311 codec (electret microphone in, PAM8403 to a speaker connector) on I2S0 and I2C0 |
| Storage | microSD socket on SD/MMC slot 0 (GPIO39–44, 4-bit) |
| Clock | DS3231 with a CR2032 |
| USB | three USB-C: the console (CH340C, auto-reset into the boot loader), the P4's full-speed and high-speed ports (the last one a Type-C host: 56 kΩ Rp on CC, 5 V to what is plugged in through an SY6280 switch limited to 750 mA) |
| Power | a Li-ion cell on IP5306 (charger and 5 V boost, its key the power button), SY8089 bucks to 3.3 V and to the P4's 1.2 V core supply |
| Board | 160 × 96 mm, 6 layers (JLCPCB JLC06161H-3313, controlled impedance), four M3 holes onto the panel's standoffs |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors,
23 warnings (the DSI and USB pairs' coupling, impedance and skew where they leave the P4's 0.35 mm
pins, 13 narrower tracks in a class, two rail names, two VSYS stubs the router leaves), 0 unrouted
nets. 179 components, 118 nets, 2644 track segments (6155 mm), 596 vias.
`analysis.run { lens: "ready" }`: 12 to fix (decoupling capacitors 3–4 mm from their P4 supply pins,
under the chip on the back) and 24 to consider.
Every assembled line has its LCSC code (chosen by stock at JLCPCB, 2026-10-25); `pcbkit export
--format fab` succeeds without `--force`. **Two parts are out of stock at JLCPCB**: the ESP32-P4NRW32
(no ESP32-P4 of any revision is; send it as a consigned part) and the 4.2 x 3.2 mm BOOT and RESET
buttons (PTS810, or any 4.2 x 3.2 mm tact switch). The ESP32-C6 is the MINI-1-H4 (the N4's 105 °C
twin, same pins).
<!-- /STATUS -->

## Run it in the browser

Open the board and press **Run**: the page fetches NucleoOS's release (its factory image and its
SD-card archive, from GitHub, checked against the SHA-256 this design declares) and Espressif's ROM,
and runs them. NucleoOS boots to its setup wizard; touch the screen in 3D to go through it to the
launcher.

What does not run, as NucleoOS itself reports it:

- **No radio.** The ESP32-C6 is on the board but held in reset: its own firmware (ESP-Hosted) does
  not run here, so NucleoOS finds no Wi-Fi ("This board has no Wi-Fi radio") and no Bluetooth.
  Nothing pretends to be a network.
- **The RTC.** NucleoOS looks for an RX8130 at 0x14; this board (like Guition's, which fits an
  RX-8025T) has a DS3231 at 0x68, so the clock starts unset.
- The PPA's blender and YUV modes, the JPEG codec and the camera are not modelled yet: an app that
  uses them stops the device and says which block.

## Order, flash

1. Order with assembly (`pcbkit export --format fab`); the panel plugs into FPC1 and FPC2.
2. Flash NucleoOS's factory image over the console port: `esptool.py --chip esp32p4 write_flash 0
   nucleoos-p4-1.2.189-jc1060p470c-factory.bin` (the CH340C's DTR and RTS put the P4 into its boot
   loader); unzip `nucleoos-p4-sdcard.zip` onto a FAT32 microSD card.

NucleoOS's chip revision: its v1.2.x images are built for ESP32-P4 revision v1.x (the image says
"max chip rev v1.99"); new ESP32-P4NRW32X parts are revision v3 and need an image built for it.

Licence: the board CC0 1.0; NucleoOS is the PolyForm Noncommercial License 1.0.0, fetched from its
own release and never copied here.

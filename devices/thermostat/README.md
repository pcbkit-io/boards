# thermostat — a 24 V AC wall thermostat on an ESP32

A thermostat for a furnace and an air conditioner on the usual four-wire hookup (R, C, W, Y, G): an
ESP32-WROOM-32E reads the room from an SHT40 every two seconds and calls heat (W), cooling (Y) and
the fan (G) through three relays, with 0.5 °C of hysteresis either side of the set point. A 0.96"
OLED shows the temperature large, the set point, the mode, the humidity and the calls; three
buttons set them. It takes its power from the furnace's own transformer (R and C).

| | |
|---|---|
| Processor | ESP32-WROOM-32E-N4 (two Xtensa LX6 cores, 4 MB flash); RESET and BOOT buttons |
| Sensor | SHT40-AD1B at 0x44 on I2C0 (GPIO21 SDA, GPIO22 SCL), at the bottom edge, away from the regulators |
| Display | 0.96" 128 × 64 SSD1306 OLED module at 0x3C on the same bus, on a 4-pin socket |
| Buttons | UP GPIO32, DOWN GPIO33, MODE GPIO25 (to ground, internal pull-ups); MODE cycles OFF / HEAT / COOL / AUTO, UP and DOWN move the set point by 0.5 °C (10–30 °C) |
| Calls | GPIO26 (W), GPIO27 (Y), GPIO14 (G) → ULN2003A → three SRD-05VDC-SL-C relays connecting R to W, Y, G |
| Power | J1: R, C (24 V AC) → bridge (4 × US1M) → SMBJ36A → LMR38020 buck (80 V, the standard `Buck` block) to 5 V (relays) → AMS1117-3.3 (logic) |
| Wiring | J1 (R, C) and J3 (W, Y, G): Phoenix MKDS 1,5 terminal blocks, 5.08 mm |
| Programming | J2: GND, TX, RX, EN, IO0 for a 3.3 V USB-serial adapter (115200 baud): hold BOOT, press RESET, `esptool.py write_flash 0x1000 U1.bin` |
| Board | 84 × 76 mm, 2 layers, four M3 holes |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 1 warning (erc-rail-name), 0 unrouted nets. 45 components, 29 nets, 280 track segments (934 mm), 30 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partial. Nothing to fix, 2 to consider, 9 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code (JLCPCB assembly).
<!-- /STATUS -->

| Finding | Why |
|---|---|
| J2 is not on an edge | the programming header, used on the bench |
| No test points | the programming header carries the console; the rails are on large parts |
| The buck needs an operating load / thermal parameters / parts not covered | the analysis' own caveats: the board draws under 0.5 W |

The supply: 24 V AC is about 34 V at its peaks after the bridge (up to 40 V on a high line). The
SMBJ36A takes the spikes, clamping them near 58 V; the LMR38020 buck takes 80 V at its input (the
LM2596 this board had took 45 V, under the TVS's clamp).

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/devices/thermostat, open the
Laboratory and press **Build & run**: the page builds `firmware/` for the ESP32 (ESP-IDF's register
headers) and runs it on a model of the chip, its I2C, GPIO and the OLED. `sim/house.js` is the house:
one room that leaks heat to the outside, heated by a 4 kW furnace while W is called and cooled by a
3 kW air conditioner while Y is (the calls read at the relay driver's inputs), its temperature
reaching the SHT40. Controls: the outside temperature, the room at the start, a time lapse, opening
the window. The buttons answer to ↑, ↓ and M.

**Wi-Fi is not here.** The ESP32's radio is not simulated (it is driven by closed libraries; the
model stops a program that turns it on and says so), and this firmware does not use it: it is the
thermostat without a network. Everything else is the program you flash.

## Order, flash, install

1. Order with assembly (JLCPCB: `pcbkit export --format fab`, or `fab.order` in the app); plug the
   OLED module into its socket.
2. Flash through J2 (`pcbkit export --format firmware` gives the image and the esptool command).
3. Turn the furnace's power off; wire R, C, W, Y, G to J1 and J3 as on the old thermostat.

Licence: CC0 1.0.

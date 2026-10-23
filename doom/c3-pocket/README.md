# 03 · c3-pocket — a Game Boy-shaped RISC-V Doom on an ESP32-C3

A small portrait handheld (52 × 128 mm) around an ESP32-C3-MINI-1 module: one RISC-V core, 400 KB
of SRAM, 4 MB of flash, a 1.3" IPS screen, eight buttons, a piezo and two 18650 cells behind it,
charged from its USB-C port.
The C3 has no PSRAM, so Doom is cut down the way rp2040-doom cut it for the RP2040.

| | |
|---|---|
| Processor | ESP32-C3-MINI-1-H4 (the -N4's 105 °C twin: same pins and 4 MB flash; the -N4 is out of stock at JLCPCB): ESP32-C3, one **RV32IMC** core (Espressif's own, 4-stage pipeline) at 160 MHz, 400 KB SRAM (16 KB of it cache), 384 KB ROM, 4 MB flash in the module (MSPI, XIP through the cache), Wi-Fi/BLE unused |
| Display | 1.3" 240 × 240 IPS, ST7789, SPI without CS (mode 3): GPIO6 SCK, GPIO7 MOSI, GPIO5 D/C, GPIO4 RES; backlight on |
| Input | 8 × 5.1 mm tact switches: d-pad IO0–IO3, fire IO10, use RXD0 (IO20), strafe TXD0 (IO21), menu IO9 (= BOOT: held at reset it enters download mode) |
| Sound | piezo on IO8 (LEDC PWM; IO8 is a strapping pin held high by 10 kΩ, the piezo is DC-open) |
| Power | 2 × 18650 Li-ion in parallel (1S2P) behind reverse-cell (AO3401A) and DW01A + FS8205A protection; BQ24075 charger with power path from USB-C (fuse, TVS); SYS → AP2112K-3.3 LDO, its enable on the slide switch |
| Programming | USB-C to the C3's own USB Serial/JTAG (IO18/IO19): `esptool.py` flashes it, no bridge chip; JTAG debugging over the same cable; RESET on the back, BOOT = MENU |
| Board | 52 × 128 mm, 2 layers, four M3 holes; front: display, buttons, USB-C (left edge, near the top), charger, switch, test pads; back: two 18650 holders, the module (top edge, antenna away from the hand), RESET, piezo |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 3 warnings (diff-pair-uncoupled, skew, track-width-class), 0 unrouted nets. 58 components, 40 nets, 502 track segments (1483 mm), 79 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partial. Nothing to fix, 4 to consider, 15 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
<!-- /STATUS -->

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/doom/c3-pocket, open the
Laboratory and press **Build & run**: the page builds `doom/` for the C3's RV32IMC core (with
rp2040-doom's `DOOM1.WHX` linked into the flash), boots it the way the chip's boot ROM boots an
image from flash, and draws the ST7789's frames on the board. Play with the keyboard (arrows,
F fire, Space use, S held to strafe, M menu; fire confirms in the menus) or a gamepad (`input.js`).

## How Doom fits

`doom/` is rp2040-doom's engine (kilograham, GPL-2: its compressed level format and its
`DOOM1.WHX`, the shareware episode in 1.8 MB) on PCBKit's ESP32-C3 kit instead of ESP-IDF: the
WHX and the constant tables read in place from the flash through the cache, the zone heap in the
DRAM (190 KB of data and bss, 70 KB of heap inside the 256 KiB its 16-bit pointers reach), the CPU
at 160 MHz. Each 320 × 200 frame is scaled by 3/4 into the panel's middle 240 × 150 (of four
columns or lines the middle two averaged), RGB565 over GP-SPI2 at 40 MHz in SPI mode 3 (the
module has no chip select): 72 KB per frame, 14 ms. The eight buttons are read from their GPIOs
(pulled up). The firmware is silent: the piezo is not driven yet.

## Power

| State | Current (estimate) |
|---|---|
| ESP32-C3 at 160 MHz, radio off | 25–30 mA |
| ST7789 1.3" with backlight | 20–30 mA |
| **Total** | **~55 mA at 3.3 V** |

Two 18650 Li-ion cells in Keystone 1042 holders on the back, upright side by side, in parallel
(1S2P, 3.0–4.2 V). From the cells to the module:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). It cannot help one cell reversed against the other: in parallel
  they would short each other, so check both against the silkscreen's + (at the top end of each
  holder) before closing a case.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack. Flat-top unprotected 18650 cells fit
  the holders.
- **USB-C input:** 5.1 kΩ on CC1 and CC2 (any USB-C charger or computer gives 5 V), a 750 mA
  resettable fuse (nSMD075), a 5 V TVS (SMF5.0A), and a USBLC6-2 ESD array on D+/D−.
- **Charger with power path:** a TI BQ24075 in USB500 mode (at most 500 mA from the port), ISET
  for 800 mA (RISET 1.13 kΩ), RILIM 1.18 kΩ, safety timers off (two cells in parallel charge longer
  than its longest timer; termination is by current), TS tied to 10 kΩ (no thermistor). A linear
  charger passes on at most what it takes in, so from USB the cells charge at up to about 500 mA.
  Its output SYS runs the board from USB when one is plugged in and from the cells otherwise.
- **3.3 V:** the AP2112K-3.3 LDO from SYS (about 25 mV dropout at this board's 55 mA): 3.3 V down
  to about 3.33 V on the cells, then it follows them; the C3 runs to 3.0 V, so the game runs until
  the cells are nearly empty.
- **On/off:** the slide switch on the right edge drives the LDO's enable (1 MΩ pulls it off); the
  charger charges with the switch off; off, the board draws only the charger's and the
  protection's few µA.

Two 3000 mAh cells (about 22 Wh) at ~0.2 W: **about 100 h** of play (estimate). A full charge of
two empty cells from USB at about 500 mA takes about 12 h.

## Charging

Plug any USB-C charger or computer into J1 (the left edge, near the top). The red LED (CHG) is on
while the cells charge and goes out when they are full; the green LED (USB) is on while USB power
is present. The board runs from USB while it charges, switched on or off. Insert the cells with +
at the end the back's silkscreen marks (the top).

## Modules plugged in by hand

The assembly solders a 1x7 female socket for the display; the module is bought separately:

- **DS1: 1.3" 240 × 240 ST7789 IPS module, SPI without CS** (7 pins: GND, VCC, SCL, SDA, RES, DC,
  BLK), its pin header soldered on as it comes, plugged in on the front.

## Check warnings and why they stay

| Warning | Why it is acceptable |
|---|---|
| `diff-pair-uncoupled` and `skew` on DP / DN (a few tens of mm uncoupled, a few mm of skew) | USB 2.0 full speed (12 Mbit/s, 83 ns a bit): the connector is on the front, the C3 on the back; tens of mm uncoupled and 6 mm of skew (about 40 ps) are nothing at this rate |
| `track-width-class` on GND: about 1 mm narrowed to 0.19 mm, at a via beside the LDO and the module's decoupling | a local ground stub between parts packed under the module; it lies inside the ground pours of both sides, which carry the ground current |

## Firmware: build and flash

- **Build:** in PCBKit, the Laboratory's **Build & run** builds `doom/` (RV32IMC, the kit's
  start-up and link layout); then export the firmware (`export.run { format: "firmware" }`):
  `U1.bin`, the whole flash from offset 0 (the boot image the ROM loads, then the program's code
  and constants and the WHX at their pages). No 2nd-stage bootloader or partition table: the ROM
  boots the image directly.
- **Flashing on arrival:** switch the board on and plug its USB-C into a computer: the C3's USB
  Serial/JTAG enumerates (303a:1001, `/dev/ttyACM0` or a COM port) even with the module's flash
  blank, and `esptool` resets the chip into its ROM download mode through it by itself:
  `esptool.py --chip esp32c3 -p /dev/ttyACM0 write_flash 0x0 U1.bin` (or the browser's ESP Web
  Flasher at offset 0). If a running program has taken the USB pins over: hold MENU (IO9, the
  C3's BOOT strap) and press RESET on the back: ROM download mode, then flash as above. Press
  RESET: Doom starts.

## Bill of materials

Rough cost at 10 pieces (estimates): ESP32-C3-MINI-1 $2.20 · ST7789 1.3" module $3.00 · AP2112K $0.15 · USB-C + ESD $0.40 · buttons $0.30 · piezo $0.30 · switch
$0.10 · two 18650 holders $9.80 · BQ24075 and protection $1.30 · PCB $2.00 → **about $22** without the cells.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 1 | BZ1 | GPC12075YB-5V4000 | C252948 | Buzzer_D12.0mm_H7.5mm_P6.50mm |
| 1 | C1 | 10uF | C15850 | C_0805_2012Metric |
| 2 | C2, C6 | 100nF | C1525 | C_0402_1005Metric |
| 3 | C3, C7, C8 | 1uF | C52923 | C_0402_1005Metric |
| 1 | D1 | tvs 5V | C284108 | D_SOD-123 |
| 1 | DS1 | Female header 1x7 2.54 mm | C2932672 | TFT_1.3in_ST7789_Socket_1x07_P2.54mm |
| 1 | F1 | PTC 680mA | C70078 | Fuse_1206_3216Metric |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P | C165948 | USB_C_Receptacle_USB2.0_16P |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 1 | R1 | 1Mohm | C26083 | R_0402_1005Metric |
| 4 | R2, R3, R4, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 2 | R5, R6 | 5.1kohm | C25905 | R_0402_1005Metric |
| 1 | R7 | 100kohm | C25741 | R_0402_1005Metric |
| 1 | R9 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R10 | 1kohm | C11702 | R_0402_1005Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | TS-1187A-B-A-B | C318884 | SW_Tactile_SPST_5.1x5.1mm_SMD |
| 1 | SW9 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 1 | SW10 | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | U1 | ESP32-C3-MINI-1-H4 | C2934569 | ESP32-C3-MINI-1_module_13.2x16.6mm_LGA_53_pads |
| 1 | U2 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U3 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U4 | AP2112K-3.3TRG1 | C51118 | SOT-23-5 |
| 1 | U5 | ESD 2-line | C7519 | SOT-23-6 |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's simulation models

The ESP32-C3 as its boot ROM leaves it after a flash boot (the image checked and loaded), the
BBPLL and the CPU clock with their rules, the watchdogs, the cache's MMU, IO_MUX and the GPIO
matrix's outputs, SYSTIMER and its interrupt through the interrupt matrix, GP-SPI2; the ST7789
(command set, addressing, a module without chip select); the buttons. The same image boots through
the chip's own ROM in Espressif's QEMU. Not modelled: GDMA, LEDC (the game is silent, as this
firmware is), USB Serial/JTAG, the radio, cache timing, the cells and the charger.

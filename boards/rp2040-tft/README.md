# 04 · rp2040-tft — the bare RP2040 with a colour screen and an SD card

Variant 02's processor without the module: the RP2040 chip with Raspberry Pi's minimal design
(12 MHz crystal, 16 MB W25Q128 QSPI flash, BOOTSEL and RESET buttons), a 1.8" colour TFT, a
microSD slot for the WAD, PWM sound through a class-D amplifier, and two 18650 Li-ion cells it
charges from its own USB-C. JLCPCB assembles everything but the display module, which plugs into
the socket it solders.

| | |
|---|---|
| Processor | RP2040 (QFN-56): 2 × **Cortex-M0+** (ARMv6-M), 133 MHz rated (rp2040-doom: 270 MHz), 264 KB SRAM, no internal flash; W25Q128JVSIQ 16 MB on the QSPI (XIP), 12 MHz crystal |
| Display | 1.8" 128 × 160 TFT, ST7735S (MSP1803), SPI0 (GPIO18 SCK, GPIO19 MOSI, GPIO17 CS, GPIO20 A0/DC, GPIO21 RESET), backlight GPIO22 |
| Storage | microSD (Hirose DM3AT) on SPI1 (GPIO10–13), card detect GPIO14 |
| Input | 8 × 6 mm tact switches: GPIO2–GPIO9 |
| Sound | GPIO15 PWM → 1 kΩ / 10 nF low-pass → PAM8403 (left channel, shutdown on GPIO16) → 8 Ω speaker on JST PH |
| Power | 2 × 18650 Li-ion in parallel (1S2P) → reverse-cell P-FET (AO3401A) → DW01A + FS8205A protection; BQ24075 charger with power path from USB-C (1 A PTC + TVS on VBUS); its SYS output feeds the TLV75533 3.3 V LDO (enabled by the slide switch) and the PAM8403 |
| Programming | USB-C to the RP2040's USB (27 Ω series, USBLC6 ESD array): hold BOOT, tap RESET → the boot ROM's UF2 drive; SWD on a 3-pin header on the back |
| Board | 132 × 104 mm, **4 layers** (GND plane on In1), four M3 holes; front: display, buttons, BOOT, RESET, on/off switch, USB-C, charge LEDs, test pads; back: the RP2040 and the rest, the two 18650 holders across the top half |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 4 warnings (track-width-class ×2, diff-pair-uncoupled, skew), 0 unrouted nets; `issues` has 4 of 4. 92 components, 69 nets, 938 track segments (2507 mm), 123 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partly by family estimates. Nothing to fix, 5 to consider, 13 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.

| Warning | Why it stays |
|---|---|
| `diff-pair-uncoupled`, `skew` (CONN_DP / CONN_DN, ~48 mm) | USB full speed (12 Mbit/s): the connector is on the front edge, the RP2040 on the back; tens of mm uncoupled and a millimetre of skew are harmless against 4–20 ns edges |
| `track-width-class` (CONN_DP / CONN_DN necked to 0.15–0.19 mm) | short necks at the 0.4 mm pitch pads of the QFN-56 and the USB-C; signals only |
| ready lens: GND's copper heats up by ~20 °C | the estimate sends the whole pack current through one 0.2 mm stub of a decoupling capacitor near the RP2040; the pack's return runs through the solid GND plane on In1 and the GND pours on both outer layers (the lens notes a pour shares the current) |
| ready lens: thermal parameters, typical scenario | estimates of the analysis, not faults of the board |
<!-- /STATUS -->

## Why it runs Doom

Same software as 02 (rp2040-doom), with three differences: 16 MB of flash (the full shareware WAD,
or Doom II in WHD form, fits beside the code), the WAD can also come from the SD card (FatFs over
SPI1, read into flash on first boot), and the video back end writes 160 × 100 (half resolution) or
a cropped 128 × 160 portrait of the 320 × 200 frame to the ST7735 as RGB565 by DMA. 128 × 160 × 2 B =
40 KB per frame at 31 MHz SPI: ~10 ms. Sound: Doom's mixer at 11 kHz into a PWM slice at ~120 kHz.

## Power

Two 18650 Li-ion cells in Keystone 1042 holders on the back, in parallel (1S2P, 3.0–4.2 V, about
5–7 Ah together), as on `rp2350-handheld`:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). It cannot help one cell reversed against the other: in parallel
  they would short each other, so check both against the silkscreen's + before closing a case.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack. Flat-top unprotected 18650 cells fit
  the holders.
- **Charger with power path:** a TI BQ24075 from the USB-C VBUS (through a 1 A PTC fuse, a 5 V TVS
  after it), in USB500 mode (at most 500 mA from the port; the CC pins' 5.1 kΩ make any Type-C
  source give 5 V), 800 mA fast charge (RISET 1.13 kΩ), input limit RILIM 1.18 kΩ, safety timers
  off (two cells charge longer than its longest timer; termination is by current), TS tied to
  10 kΩ (no thermistor). Its output SYS runs the board from USB when one is plugged in and from
  the cells otherwise.
- **Rails:** SYS (3.0–5.5 V) feeds the TLV75533 LDO (3.3 V, 500 mA; it follows the cells below about
  3.55 V, where the RP2040, the flash, the card and the display still run) and the PAM8403 directly.
  The slide switch on the left edge enables the LDO (100 kΩ holds it off); a 100 kΩ pull-down keeps
  the amplifier shut down while the RP2040 is off. The cells keep charging with the switch off.

| State | Current (estimate) |
|---|---|
| RP2040 at 270 MHz, both cores | ~50 mA |
| ST7735 with backlight | ~25 mA |
| SD card reading (bursts) | 20–60 mA |
| PAM8403 at moderate volume | 30–100 mA from SYS |
| **Typical** | **~100 mA at 3.7 V** |

Two 3000 mAh cells → **about 55 h** of play (estimate). A full charge of two empty cells at 800 mA
takes about 8 h (longer from a 500 mA USB 2.0 port).

## Charging

Plug a USB-C charger or computer into the USB-C on the bottom edge. The red LED (CHG) is on while
the cells charge and goes out when they are full; the green LED (USB) is on while USB power is
present. The board runs from USB while it charges, switched on or off. Insert both cells with +
at the end the back's silkscreen marks.

## Flashing on arrival

Nothing to solder and no programmer: the RP2040's boot ROM has a USB loader.

1. Connect the USB-C to a computer.
2. Hold **BOOT**, tap **RESET**, release BOOT (or hold BOOT while plugging in): the board appears
   as the **RPI-RP2** drive.
3. Copy `doom.uf2` onto it; the RP2040 writes it into the W25Q128 and reboots into it.

Or over SWD: the 3-pin header on the back (SWCLK, GND, SWDIO) takes a Raspberry Pi Debug Probe:
`openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "program doom.elf verify reset exit"`.
The game data goes on a FAT32 microSD card (DOOM1.WAD at its root).

## Modules plugged in by hand

- **DS1: 1.8" 128 × 160 ST7735S SPI TFT module** (MSP1803 pinout, 8 pins: VCC, GND, CS, RESET,
  A0, SDA, SCK, LED) into the 1x8 socket JLCPCB solders on the front.
- An **8 Ω speaker** (0.5–2 W) with a JST PH 2-pin plug into J5 at the right edge of the back.
- Two **18650 cells** and a **microSD card**.

## Firmware: build

- Pico SDK 2.x (`PICO_BOARD` a custom header: W25Q128, 12 MHz, `PICO_FLASH_SIZE_BYTES 16M`),
  `arm-none-eabi-gcc`; rp2040-doom with an ST7735 + FatFs back end (not written yet: see the
  repository's README).

## Bill of materials

Rough cost at 10 pieces (estimates): RP2040 $0.80 · W25Q128 $1.00 · crystal $0.30 · ST7735 module
$3.00 · microSD socket $0.60 · PAM8403 $0.15 · BQ24075 $1.10 · DW01A + FS8205A + AO3401A $0.25 ·
TLV75533 $0.20 · USB-C + ESD + TVS + PTC $0.50 · buttons $3.00 · passives $1.00 · two 18650 holders
$9.80 · 4-layer PCB $3.00 · speaker $1.00 → **about $26** without the cells, plus assembly.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 12 | C1, C2, C3, C4, C5, C9, C12, C13, C14, C15, C18, C19 | 100nF | C1525 | C_0402_1005Metric |
| 1 | C6 | 10uF | C19702 | C_0603_1608Metric |
| 1 | C7 | 10nF | C15195 | C_0402_1005Metric |
| 6 | C10, C11, C24, C25, C26, C28 | 1uF | C52923 | C_0402_1005Metric |
| 2 | C16, C17 | 15pF | C1548 | C_0402_1005Metric |
| 3 | C22, C23, cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | C27 | 22uF | C12891 | C_1206_3216Metric |
| 1 | C29 | 470nF | C47339 | C_0402_1005Metric |
| 1 | D1 | tvs 5V | C918844 | D_SOD-323 |
| 1 | DS1 | Female header 1x8 2.54 mm | C2905417 | TFT_1.8in_ST7735_Socket_1x08_P2.54mm |
| 1 | F1 | PTC 1000mA | C20800 | Fuse_1206_3216Metric |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P | C165948 | USB_C_Receptacle_USB2.0_16P |
| 1 | J3 | PinHeader_1x03_P2.54mm_Vertical | C49257 | PinHeader_1x03_P2.54mm_Vertical |
| 1 | J4 | DM3AT-SF-PEJM5 | C114218 | Hirose_DM3AT_microSD_socket_push-push |
| 1 | J5 | S2B-PH-K-S(LF)(SN) | C173752 | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 4 | R1, R10, R17, rBoot | 1kohm | C11702 | R_0402_1005Metric |
| 2 | R2, R3 | 27ohm | C25100 | R_0402_1005Metric |
| 6 | R4, R6, R7, R8, R9, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 3 | R11, R12, R15 | 100kohm | C25741 | R_0402_1005Metric |
| 2 | R13, R14 | 5.1kohm | C25905 | R_0402_1005Metric |
| 1 | R16 | 100ohm | C25076 | R_0402_1005Metric |
| 10 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, boot, reset | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW9 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 1 | U1 | RP2040 | C2040 | QFN-56-1EP_7x7mm_P0.4mm_EP3.1x3.1mm |
| 1 | U2 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U3 | W25Q128JVSIQ | C97521 | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U4 | TLV75533PDBVR | C404027 | SOT-23-5 |
| 1 | U5 | PAM8403DR | C5122557 | SOIC-16_3.9x9.9mm_P1.27mm |
| 1 | U6 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U7 | ESD 2-line | C7519 | SOT-23-6 |
| 1 | Y1 | 12MHz 10pF | C20625731 | Crystal_SMD_3225-4Pin |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

Everything of variant 02, plus: the boot ROM's **USB mass-storage UF2 loader** if flashing is to be
simulated; the **SSI/XIP with 16 MB** (the current SoC maps 2 MB); **two PL022 SPI** blocks with
**DMA** (paced by the SPI's DREQ); the **PWM slice** as an audio source (duty → sample); an SD card
model in SPI mode (CMD0/8/55/41/58/17/18/24, a FAT image as its contents); an ST7735 model; the
BQ24075/DW01A/LDO power chain as a two-cell battery model with protection thresholds, the power
path and the charge current.

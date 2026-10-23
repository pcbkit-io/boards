# 10 · teensy41 — Cortex-M7 at 600 MHz: Doom at full speed, Doom II too

The most capable variant: a Teensy 4.1 (NXP i.MX RT1062) on sockets behind a 3.5" 320 × 480 touch
TFT, ten buttons, CD-quality audio to headphones and a speaker, and two 18650 cells with protection,
a power-path charger on USB-C, a fuel gauge and a 5 V boost. JLCPCB assembles everything but the two
modules (and the speaker), which plug into the sockets and the connector it solders. The i.MX RT1062 is a BGA-196 crossover MCU with external flash; the Teensy module is
how hobbyists (and small products) use it without a 6-layer BGA board.

| | |
|---|---|
| Processor | Teensy 4.1: NXP i.MX RT1062, Arm **Cortex-M7** (ARMv7E-M, 6-stage dual-issue, double-precision FPU, 32 KB I + 32 KB D cache) at 600 MHz; 1 MB SRAM (512 KB tightly coupled ITCM/DTCM); 8 MB QSPI flash (FlexSPI); pads for 8 MB PSRAM (FlexSPI2); microSD socket on 4-bit SDIO (uSDHC1) |
| Display | 3.5" 320 × 480 ILI9488 + XPT2046 touch (MSP3520) on LPSPI4 (D13 SCK, D11 MOSI, D12 MISO, D10 CS, D9 DC, D8 RESET, D6 backlight PWM; touch CS D5, IRQ D4) |
| Input | 10 × 6 mm tact switches: D0–D3, D14–D17, D22, D24 |
| Sound | SAI1 I2S (D21 BCLK, D20 LRCLK, D7 TX) → **PCM5102A** → 3.5 mm jack and → **PAM8403** (shutdown on D26) → 8 Ω speaker |
| Power | 2 × 18650 Li-ion in parallel (1S2P) behind reverse-cell (AO3401A) and DW01A + FS8205A protection; USB-C (charging only, 1 A PTC fuse, TVS) → **BQ24075** power path (SYS); **MAX17048** fuel gauge (D18 SDA, D19 SCL, alert D25); **TPS61023** boost SYS → 5 V (switched by the on/off switch on its EN) → 1N5819HW → Teensy VIN (VIN and VUSB are joined on the Teensy: the diode keeps the boost and a USB host apart) |
| Programming | the Teensy's micro-USB (not the charging USB-C): **Teensy Loader** / `teensy_loader_cli` talking to its MKL02 bootloader chip (HalfKay); the program button forces it |
| Board | 172 × 112 mm, **4 layers**, four M3 holes; front: display, buttons, on/off switch, charging USB-C, charger and boost, test pads; back: Teensy (its USB at the top edge), two 18650 holders (right of the Teensy, so its microSD card comes out), audio, speaker plug (left edge), jack (right edge) |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 0 warnings, 0 unrouted nets. 88 components, 66 nets, 769 track segments (3204 mm), 43 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partly by family estimates. Nothing to fix, 1 to consider, 14 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
<!-- /STATUS -->

The ready lens's notes are not faults: it estimates heat with generic package limits where the
makers publish no thermal data, and reads the Teensy's and the screen's draw from their families.
Keystone 1042 holders (C5249310) are stocked in hundreds at JLCPCB: check before ordering more than
a few boards.

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/doom-handheld/tree/main/boards/teensy41, open the
Laboratory and press **Build & run**: the page builds `doom/` for the i.MX RT1062's Cortex-M7
(with `DOOM1.WAD` linked into its flash), boots it the way the chip's boot ROM boots the Teensy's
flash, and draws the ILI9488's frames on the board. Play with the keyboard (arrows, F fire, Space
use, comma/period strafe, M menu, Enter) or a gamepad (`input.js`).

## Why it runs Doom

The Cortex-M7 at 600 MHz runs Doom with time to spare; the game's data, the shareware WAD, is
linked into the Teensy's 8 MB flash with the program and read in place, so no SD card is needed.
Its 1 MB of RAM holds the rest: the program's start-up makes the FlexRAM one OCRAM with OCRAM2
(1 MB from 0x2020_0000) for the engine's state, two screens and a zone heap of about 600 KB (Doom's
shareware levels need about 576 KB, measured). For that the board's copy of the engine keeps its
trigonometric tables in flash, its visplanes' rows in bytes (as Doom's original did) and changes
screens without the melt (`doom/doom_teensy.c` says where). Each frame goes to the ILI9488 in
18-bit colour (its only format over SPI), 320 × 200 in the middle of its 480 × 320, on LPSPI4 at
33 MHz: 46 ms a frame. Measured in PCBKit's simulator: the title at 4 s of device time, the demo
at 13 s.

## Power

Two 18650 Li-ion cells in Keystone 1042 holders on the back, in parallel (1S2P, 3.0–4.2 V), the
power section of `boards/rp2350-handheld` with this board's own USB-C, boost and fuel gauge:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). It cannot help one cell reversed against the other: in parallel
  they would short each other, so check both against the silkscreen's + before closing a case.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack. Flat-top unprotected 18650 cells fit the
  holders.
- **Charger with power path:** a TI BQ24075 from the USB-C (5.1 kΩ on CC, a 1 A resettable fuse and
  a TVS diode on VBUS), in USB500 mode (at most 500 mA from the port), 800 mA fast charge (RISET
  1.13 kΩ), input limit set by RILIM 1.18 kΩ, safety timers off (two cells in parallel charge longer
  than its longest timer; termination is by current), TS tied to 10 kΩ (no thermistor). Its output
  SYS runs the board from USB when one is plugged in and from the cells otherwise.
- **5 V and on/off:** the TPS61023 boosts SYS to 5 V for the Teensy's VIN through a 1N5819HW; the
  slide switch on the left edge drives the boost's EN (off: the Teensy unpowered, the cells still
  charging). The speaker amplifier runs from SYS, the DAC and the screen from the Teensy's 3.3 V.
- **Fuel gauge:** a MAX17048 on the cells (after the protection) reports their charge over I2C.

| State | Current (estimate) |
|---|---|
| Teensy 4.1 at 600 MHz | ~100 mA at 5 V VIN (PJRC: about 100 mA) |
| ILI9488 3.5" with backlight | ~80–100 mA at 3.3 V |
| PCM5102A + PAM8403 | 20 + 50–200 mA |
| **Typical** | **≈ 1 W → ~300 mA at 3.7 V after the boost** |

Two 3000 mAh cells → **about 20 h** (estimate). A full charge of two empty cells at 800 mA takes
about 8 h (longer from a USB 2.0 port, which the charger limits to 500 mA).

## Charging

Plug a USB-C charger into the board's USB-C at the bottom edge (charging only; it has no data). The
red LED (CHG) is on while the cells charge and goes out when they are full; the green LED (USB) is
on while USB power is present. The board plays from USB while it charges, and charges with the
switch off. Insert the cells with + at the end the back's silkscreen marks.

## Modules plugged in by hand

The assembly solders two 1x24 female sockets, one 1x14 female socket and a JST PH speaker plug;
these are bought separately:

- **U1: Teensy 4.1** (PJRC TEENSY41, with pins soldered on), on the back, components facing the
  board, its USB at the top edge; a microSD card in its socket holds the WAD.
- **DS1: 3.5" ILI9488 SPI TFT with touch, MSP3520** (14 pins), on the front.
- **J5: an 8 Ω, 1–2 W speaker** with a 2-pin JST PH lead.

## Firmware: build and flash

- **Build:** in PCBKit, the Laboratory's **Build & run** builds `doom/` (Cortex-M7, hard float, the
  kit's flash configuration block, IVT and boot data at the start of flash); then export the
  firmware (`export.run { format: "firmware" }`): `U1.hex`.
- **Flashing on arrival:** plug the Teensy's micro-USB into a computer, open the Teensy Loader (or
  run `teensy_loader_cli --mcu=TEENSY41 -w -v U1.hex`) and press the Teensy's program button: its
  MKL02 boot loader chip writes the image; nothing on the board to configure, no programmer.

## Bill of materials

Rough cost at 10 pieces (estimates): Teensy 4.1 $31.50 · 2 × 24-pin sockets $0.80 · MSP3520 3.5"
touch module $11 · PCM5102A $2.00 · PAM8403 $0.15 · jack $0.60 · BQ24075 $1.10 · DW01A, FS8205A and
AO3401A $0.25 · MAX17048 $1.60 · TPS61023 $0.60 · USB-C $0.20 · buttons $2.90 · passives $1.50 ·
two 18650 holders $9.80 · 4-layer PCB $5.00 · speaker $1.00 · microSD card $4 → **about $75**
without the cells.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 6 | C1, C3, C5, C9, C18, C21 | 100nF | C1525 | C_0402_1005Metric |
| 4 | C2, C4, C6, C22 | 10uF | C15850 | C_0805_2012Metric |
| 2 | C7, C8 | 2.2uF | C87994 | C_0805_2012Metric |
| 2 | C10, C11 | 2.2nF | C1531 | C_0402_1005Metric |
| 4 | C12, C13, C14, C16 | 1uF | C52923 | C_0402_1005Metric |
| 3 | C15, C23, C24 | 22uF | C12891 | C_1206_3216Metric |
| 1 | C17 | 470nF | C47339 | C_0402_1005Metric |
| 1 | D1 | 1N5819HW-7-F | C82544 | D_SOD-123 |
| 1 | D2 | tvs 5V | C19224 | D_SOD-323 |
| 1 | DS1 | Female header 1x14 2.54 mm | C2905420 | TFT_3.5in_ILI9488_Socket_1x14_P2.54mm |
| 1 | F1 | PTC 1000mA | C20800 | Fuse_1206_3216Metric |
| 1 | J1 | SJ1-3525N | C4991936 | Jack_3.5mm_SameSky_SJ1-352XN_Horizontal |
| 1 | J3 | USB_C_Receptacle_USB2.0_16P | C165948 | USB_C_Receptacle_USB2.0_16P |
| 1 | J5 | S2B-PH-K-S(LF)(SN) | C173752 | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | L1 | 1uH | C96955 | L_Power_6x6mm |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 5 | R1, R2, R3, R4, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 1 | R5 | 1Mohm | C26083 | R_0402_1005Metric |
| 2 | R6, R7 | 470ohm | C25117 | R_0402_1005Metric |
| 2 | R8, R18 | 100kohm | C25741 | R_0402_1005Metric |
| 1 | R9 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R10 | 1kohm | C11702 | R_0402_1005Metric |
| 2 | R11, R12 | 5.1kohm | C25905 | R_0402_1005Metric |
| 2 | R15, R16 | 3.3kohm | C25890 | R_0402_1005Metric |
| 1 | R17 | 732kohm | C137940 | R_0402_1005Metric |
| 10 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10 | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW12 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 2 | U1 | Female header 1x24 2.54 mm | C41376163 | Teensy41_Socket_2x24_P2.54mm_W15.24mm |
| 1 | U2 | PCM5102APWR | C107671 | TSSOP-20_4.4x6.5mm_P0.65mm |
| 1 | U3 | PAM8403DR | C17337 | SOIC-16_3.9x9.9mm_P1.27mm |
| 1 | U4 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U5 | MAX17048G+T10 | C2682616 | DFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm |
| 1 | U6 | TPS61023DRLR | C919459 | SOT-563 |
| 1 | U7 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** **ARMv7E-M with FPv5-D16** (double precision), plus the M7's **caches** (CCR, cache
  maintenance by address: the SDK cleans/invalidates around DMA) and **TCM** (ITCM/DTCM at 0x0 /
  0x2000_0000 with FlexRAM banks configured by `IOMUXC_GPR`); MPU regions the SDK programs.
- **Interrupt controller:** NVIC with 160 lines; SysTick; the GPT/PIT timers Teensyduino uses.
- **Boot ROM:** the i.MX RT boot ROM's **serial NOR boot**: read the FCB at 0x6000_0000 to configure
  FlexSPI, then the IVT and boot data, then jump; the image format is that header, not ELF.
- **Clocks (CCM):** ARM PLL to 600 MHz, the dividers for IPG/PERCLK, LPSPI and SAI root clocks;
  the DCDC voltage step Teensyduino performs before raising the clock.
- **Peripherals:** **IOMUXC** pin mux (every pad has a mux register and a pad-control register: the
  pin-mux table from NXP's SVD), GPIO1–9 (fast GPIO6–9 aliases), LPSPI4 + **eDMA with DMAMUX**,
  **SAI1** (I2S) + eDMA, LPI2C1, **uSDHC1** (SD 4-bit with ADMA2 descriptors), USB OTG (the bootloader
  is a separate chip: flashing can be modelled as writing the image directly).
- **Board:** ILI9488 (18-bit pixel format over SPI) + XPT2046, SD card in SD mode, PCM5102A, BQ24075,
  MAX17048, two 18650 cells as a discharge curve, a boost into VIN behind a diode.

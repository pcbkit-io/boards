# 06 · pico2-carrier — the reference device as an orderable carrier

PCBKit's reference Doom device, made orderable: a carrier for the **Pimoroni Pico Plus 2**
(RP2350B, 16 MB flash, **8 MB PSRAM** on QMI chip select 1 = GPIO47; the full game's data and heap
need both, a plain Pico 2 has neither room) on the Pico's 2 × 20 sockets — with
the same 2.8" ILI9341 touch TFT (MSP2807) and ten buttons as the reference, plus a PCM5102A DAC and a
headphone jack. It runs from two 18650 Li-ion cells on its back, charged from the module's USB.
JLCPCB assembles everything but the two modules, which plug into the sockets it solders.

| | |
|---|---|
| Processor | RP2350B on the Pico Plus 2: 2 × **Cortex-M33** (ARMv8-M Mainline, DSP, single-precision FPU, TrustZone) **or** 2 × **Hazard3** (RV32IMAC + Zba/Zbb/Zbs/Zbkb/Zcb/Zcmp), chosen by the image; 150 MHz; 520 KB SRAM; 16 MB QSPI flash and 8 MB PSRAM through the QMI's 16 KB XIP cache |
| Display | 2.8" 240 × 320 ILI9341 with XPT2046 resistive touch on SPI0 (GP18 SCK, GP19 MOSI, GP16 MISO, GP17 CS, GP20 DC, GP21 RESET, GP22 backlight; touch CS GP13, IRQ GP14) |
| Input | 10 × 6 mm tact switches, as the reference: d-pad GP0–GP3, fire GP4, use GP5, strafe L/R GP6/GP7, menu GP8, enter GP9 |
| Sound | PIO I2S (GP10 BCK, GP11 LRCK, GP12 DIN) → PCM5102A (hardware mode, PLL from BCK, mute on GP15) → 470 Ω / 2.2 nF → 3.5 mm stereo jack |
| Power | 2 × 18650 Li-ion in parallel (1S2P, 3.0–4.2 V) behind reverse-cell (AO3401A) and DW01A + FS8205A protection; BQ24075 charger with power path from the module's USB; SYS → 1N5819HW → module VSYS; the module's buck-boost makes 3.3 V (up to 300 mA for the carrier) |
| Programming | the module's USB: BOOTSEL + plug → UF2 (RP2350's ROM accepts ARM and RISC-V images); SWD on the module's debug pads; the same port charges |
| Board | 160 × 100 mm, 2 layers, four M3 holes; front: display, ten buttons, charger, on/off switch, test pads; back: module (USB at the top edge), two 18650 holders, DAC, jack (right edge) |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 0 warnings, 0 unrouted nets. 62 components, 53 nets, 504 track segments (2876 mm), 54 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partly by family estimates. Nothing to fix, 1 to consider, 12 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
<!-- /STATUS -->

The ready lens's two notes are not faults: it estimates heat with generic package limits where the
modules' makers publish no thermal data, and reads the module's draw from its family (the board
dissipates well under 1 W). Keystone 1042 holders (C5249310) are stocked in hundreds at JLCPCB:
check before ordering more than a few boards.

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/doom/pico2-carrier, open the
Laboratory and press **Run**: the page builds `doom/` for the RP2350 (with `DOOM1.WAD` linked into
flash), boots the image on a register-level model of the chip and its PSRAM, and draws the
display's frames on the board. Play with the keyboard (arrows, F fire, Space use, comma/period
strafe, M menu, Enter) or a gamepad (`input.js` maps a standard controller).

## Why it runs Doom

The Pico Plus 2's 8 MB of PSRAM is mapped at 0x1100_0000 through the XIP cache, so Doom runs as an
ordinary program: the zone heap and the screens in PSRAM, the full DOOM1.WAD in flash, on one
Hazard3 core at 150 MHz. The display takes each 320 × 200 frame as RGB565 over SPI0 at 25 MHz.

## Power

Two 18650 Li-ion cells in Keystone 1042 holders on the back, in parallel (1S2P, 3.0–4.2 V), the same
power section as `doom/rp2350-handheld`:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). It cannot help one cell reversed against the other: in parallel
  they would short each other, so check both against the silkscreen's + before closing a case.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack. Flat-top unprotected 18650 cells fit the
  holders.
- **Charger with power path:** a TI BQ24075 from the module's VBUS, in USB500 mode (at most 500 mA
  from the port), 800 mA fast charge (RISET 1.13 kΩ), input limit set by RILIM 1.18 kΩ, safety
  timers off (two cells in parallel charge longer than its longest timer; termination is by
  current), TS tied to 10 kΩ (no thermistor). Its output SYS runs the board from USB when one is
  plugged in and from the cells otherwise; SYS reaches the module's VSYS through a 1N5819HW
  Schottky, beside the module's own VBUS diode.
- **On/off:** the slide switch on the left edge pulls the module's 3V3_EN low (its buck-boost off;
  the charger still charges with the switch off).

| State | Current at 3.3 V (estimate) |
|---|---|
| RP2350 at 150 MHz + PSRAM | ~40 mA |
| ILI9341 2.8" with backlight | ~60–80 mA |
| PCM5102A | ~20 mA |
| **Total** | **~130 mA at 3.3 V ≈ 0.5 W with losses → ~130 mA from 3.7 V** |

Two 3000 mAh cells → **about 45 h** of play (estimate). A full charge of two empty cells at 800 mA
takes about 8 h (longer from a USB 2.0 port, which the charger limits to 500 mA).

## Charging

Plug a USB charger or computer into the module's USB. The red LED (CHG) is on while the cells
charge and goes out when they are full; the green LED (USB) is on while USB power is present. The
board runs from USB while it charges, switched on or off. Insert the cells with + at the end the
back's silkscreen marks.

## Modules plugged in by hand

The assembly solders two 1x20 female sockets and one 1x14 female socket; the modules are bought
separately and plugged in:

- **U1: Pimoroni Pico Plus 2** (PIM724, with headers): on the back, components facing the board, the USB end
  at the top edge.
- **DS1: 2.8" ILI9341 SPI TFT with touch, MSP2807** (14 pins), on the front.

## Firmware: build and flash

- `doom/` is the program: `doom_rp2350.c` is the platform layer (clocks, the PSRAM, the display on
  SPI0, the buttons, the timer), the engine is sysprog21/doom_riscv, unchanged. Pins: display SCK
  GP18, MOSI GP19, CS GP17, D/C GP20, RESET GP21, backlight GP22, the touch's CS GP13 held high;
  buttons GP0 … GP9 (up, down, left, right, fire, use, strafe left, strafe right, menu, enter).
  No console (GP0 and GP1 are buttons) and no sound yet (the DAC stays silent).
- **Build:** in PCBKit, the Laboratory's **Run** builds it (RISC-V, `DOOM1.WAD` linked into flash);
  then export the firmware (`export.run { format: "firmware" }`, or the app's export menu): `U1.uf2`.
- **Flashing on arrival:** hold the module's BOOTSEL button and plug its USB into a computer: it
  appears as the RP2350 drive; copy `U1.uf2` onto it and it reboots into the game; or
  `picotool load -x U1.uf2`. Nothing on the board to configure.

## Bill of materials

Rough cost at 10 pieces (estimates): Pimoroni Pico Plus 2 $13 · 2 × 20-pin sockets
$0.50 · ILI9341 2.8" touch module $7.00 · PCM5102A $2.00 · jack $0.60 · 10 buttons $2.90 · charger,
protection and passives $2.00 · two 18650 holders $9.80 · PCB $2.00 → **about $40** without the cells.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 5 | C1, C3, C5, C9, C12 | 100nF | C1525 | C_0402_1005Metric |
| 3 | C2, C4, C6 | 10uF | C15850 | C_0805_2012Metric |
| 2 | C7, C8 | 2.2uF | C87994 | C_0805_2012Metric |
| 2 | C10, C11 | 2.2nF | C1531 | C_0402_1005Metric |
| 1 | D1 | 1N5819HW-7-F | C82544 | D_SOD-123 |
| 1 | DS1 | Female header 1x14 2.54 mm | C2905420 | TFT_2.8in_ILI9341_Socket_1x14_P2.54mm |
| 1 | J1 | SJ1-3525N | C4991936 | Jack_3.5mm_SameSky_SJ1-352XN_Horizontal |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 4 | R1, R2, R3, R4 | 10kohm | C25804 | R_0603_1608Metric |
| 2 | R5, R6 | 470ohm | C25117 | R_0402_1005Metric |
| 1 | R7 | 100kohm | C25741 | R_0402_1005Metric |
| 1 | R8 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R9 | 1kohm | C11702 | R_0402_1005Metric |
| 10 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10 | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW11 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 2 | U1 | Female header 1x20 2.54 mm | C2905423 | Raspberry_Pi_Pico_2_Socket_2x20_P2.54mm_W17.78mm |
| 1 | U2 | PCM5102APWR | C107671 | TSSOP-20_4.4x6.5mm_P0.65mm |
| 1 | U3 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U4 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 2 | cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
| 1 | rTs | 10kohm | C25744 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Cores:** Hazard3 (RV32IMAC + the bit-manipulation and Zcb/Zcmp extensions, machine and user modes,
  PMP) — the existing RV32IMAC core plus extensions; **or** Cortex-M33 (ARMv8-M Mainline: Thumb-2 as
  for M4, FPU, stack limit registers, the security extension the SDK sets up in Secure state).
- **Interrupt controllers:** Hazard3's **Xh3irq** (an external-interrupt controller in custom CSRs:
  `meiea`, `meipa`, `meinext`… — the `Bus::csr` hook exists for exactly this) and the M33's NVIC.
- **Boot ROM:** RP2350's ROM searches flash for an IMAGE_DEF in the first 4 KB, chooses ARM or
  RISC-V, sets up XIP, handles BOOTSEL (USB UF2 + PICOBOOT) and the OTP; a model needs at least the
  image-definition parser and the XIP setup the ROM leaves behind.
- **Memory:** the **QMI** with two chip selects (flash on CS0, PSRAM on CS1/GPIO47), its timing and
  the 16 KB XIP cache; PSRAM writable through the cache.
- **Clocks:** XOSC 12 MHz, PLL_SYS 150 MHz, clk_peri, clk_hstx; the TICKS block for timers.
- **Peripherals:** PL022 SPI0 + **DMA** (the display), **PIO** (I2S), TIMER0/1, IO_BANK0/PADS with
  the RP2350's funcsel table, SIO (FIFOs, spinlocks, the RISC-V/ARM-agnostic doorbells), RESETS,
  the watchdog's scratch registers (the ROM's reboot path).
- **Board:** ILI9341 + XPT2046 models on one SPI bus with two chip selects, the PCM5102A as an I2S
  sink, and two 18650 cells as a discharge curve behind the charger's power path.

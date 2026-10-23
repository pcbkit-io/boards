# 07 · rp2350-psram — the bare RP2350A with QSPI flash and PSRAM on one bus

The reference device's chip without the module: an RP2350A with Raspberry Pi's minimal design
(core switching regulator, 12 MHz crystal), a 16 MB W25Q128 on QSPI chip select 0 and an 8 MB
APS6404L PSRAM on the QMI's chip select 1 (GPIO8) sharing the same SCLK and SD0–SD3, so both are
memory-mapped through the XIP cache and Doom runs as an ordinary program. Two 18650 Li-ion cells
power it, behind reverse-cell and DW01A protection, charged from the USB-C by a BQ24075 with power
path, with a MAX17048 fuel gauge and a switched LDO. JLCPCB assembles everything but the display
module, which plugs into the socket it solders.

| | |
|---|---|
| Processor | RP2350A (QFN-60): 2 × **Cortex-M33** (ARMv8-M Mainline, FPU, DSP, TrustZone) **or** 2 × **Hazard3** RISC-V (RV32IMAC + Zba/Zbb/Zbs/Zbkb/Zcb/Zcmp), the architecture chosen by the image; 150 MHz; 520 KB SRAM; on-chip 1.1 V switching regulator (3.3 µH) |
| Memory | W25Q128JVSIQ 16 MB on QSPI_SS (CS0); APS6404L-3SQR 8 MB PSRAM on GPIO8 = QMI CS1 (XIP_CS1n); both mapped by the QMI (flash at 0x1000_0000, PSRAM at 0x1100_0000), 16 KB XIP cache |
| Display | 2.0" 240 × 320 IPS, ST7789 (MSP2008), SPI0 (GPIO18 SCK, GPIO19 MOSI, GPIO17 CS, GPIO16 DC, GPIO20 RES, GPIO21 backlight); landscape 320 × 240 |
| Storage | microSD on SPI1 (GPIO10–13), card detect GPIO14 |
| Input | 10 × 6 mm tact switches: GPIO0–5 (d-pad, fire, use), GPIO9, GPIO15 (strafe), GPIO25 (menu), GPIO26 (enter) |
| Sound | PIO I2S (GPIO22 BCK, GPIO23 LRCK, GPIO24 DIN) → MAX98357A → 8 Ω speaker |
| Power | 2 × 18650 Li-ion in parallel (1S2P) → reverse-cell P-FET (AO3401A) → DW01A + FS8205A; USB-C (1 A PTC + TVS) → **BQ24075** (power path SYS; USB500 mode, 800 mA charge) → TLV75533 (EN on the slide switch) → 3.3 V; **MAX17048** gauge on the pack, on I2C1 (GPIO6 SDA, GPIO7 SCL, 0x36, alert GPIO29); amplifier on SYS |
| Programming | USB-C to the RP2350's USB (27 Ω, USBLC6 ESD array): hold BOOT (1 kΩ on QSPI_SS), tap RESET → the boot ROM's UF2 drive, for ARM or RISC-V images; SWD + RUN on a 4-pin header on the back |
| Board | 144 × 110 mm, **4 layers** (In1 a solid GND plane), four M3 holes; front: display, buttons, BOOT, RESET, on/off switch, USB-C, charge LEDs, test pads; back: the RP2350 and the rest, the two 18650 holders across the top half. Net classes: 0.15 mm tracks and gaps with 0.45 / 0.2 mm vias by default (the QFN-60's 0.4 mm pitch), 0.3 mm for 3.3 V and the core, 0.6 mm with 0.8 / 0.4 mm vias for the power paths |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 3 warnings (diff-pair-uncoupled, skew, track-width-class), 0 unrouted nets; `issues` has 3 of 3. 100 components, 75 nets, 1224 track segments (3267 mm), 166 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partly by family estimates. Nothing to fix, 5 to consider, 14 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.

| Warning | Why it stays |
|---|---|
| `diff-pair-uncoupled`, `skew` (DP / DN, ~60 mm) | USB full speed (12 Mbit/s): the connector is on the front edge, the RP2350 on the back; tens of mm uncoupled and ten of skew (~70 ps) are harmless against 4–20 ns edges |
| `track-width-class` (DP necked to 0.15 mm) | a short neck at the 0.4 mm pitch pads; a signal |
| ready lens: J3 is not on an edge | the SWD header is for a debug probe's jumper wires, not a cable to the outside |
| ready lens: thermal parameters, typical scenario | estimates of the analysis, not faults of the board |
<!-- /STATUS -->

## Why it runs Doom

This is the reference device's software on its own board: Doom with its zone heap and screens in
the PSRAM, the WAD in flash (16 MB: the shareware or registered WAD linked as data), on one Hazard3
core at 150 MHz; the second core drives the display (320 × 240 RGB565 = 153 KB by SPI DMA at 62.5
MHz: ~20 ms). Estimated at 25–35 fps (not measured on hardware; PCBKit's simulated reference runs
its Doom at 35 fps with one instruction per clock and no memory latency).

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
- **Charger with power path:** a TI BQ24075 (the BQ24074's sibling with SYSOFF and a fixed
  current-based termination) from the USB-C VBUS (through a 1 A PTC fuse, a 5 V TVS after it), in
  USB500 mode (at most 500 mA from the port; 5.1 kΩ on CC), 800 mA fast charge (RISET 1.13 kΩ),
  input limit RILIM 1.18 kΩ, safety timers off (two cells charge longer than its longest timer),
  TS tied to 10 kΩ (no thermistor). Its output SYS runs the board from USB when one is plugged in
  (flat or missing cells included) and from the cells otherwise.
- **Fuel gauge:** the MAX17048 measures the pack after the protection (its ModelGauge needs no
  sense resistor; two cells in parallel read as one cell of twice the capacity).
- **Rails:** SYS feeds the TLV75533 LDO (3.3 V; it follows the cells below about 3.55 V) and the
  MAX98357A directly; the slide switch on the left edge enables the LDO (100 kΩ holds it off), the
  amplifier is shut down by its SD_MODE while the RP2350 is off. The cells keep charging with the
  switch off.

| State | Current (estimate) |
|---|---|
| RP2350 at 150 MHz + PSRAM + flash | ~45 mA at 3.3 V |
| ST7789 2.0" with backlight | ~40 mA |
| MAX98357A | 20–200 mA from SYS |
| **Typical** | **~120 mA at 3.7 V** |

Two 3000 mAh cells → **about 50 h** of play (estimate). A full charge of two empty cells at 800 mA
takes about 8 h (longer from a 500 mA USB 2.0 port).

## Charging

Plug a USB-C charger or computer into the USB-C on the bottom edge. The red LED (CHG) is on while
the cells charge and goes out when they are full; the green LED (USB) is on while USB power is
present. The board runs from USB while it charges, switched on or off. Insert both cells with +
at the end the back's silkscreen marks. The firmware reads the charge from the MAX17048 (0x36).

## Flashing on arrival

Nothing to solder and no programmer: the RP2350's boot ROM has a USB loader.

1. Connect the USB-C to a computer.
2. Hold **BOOT**, tap **RESET**, release BOOT (or hold BOOT while plugging in): the board appears
   as the **RP2350** drive.
3. Copy `doom.uf2` onto it (an Arm or a RISC-V image: the boot ROM runs either); or
   `picotool load -x doom.uf2` with the board in BOOTSEL.

Or over SWD: the 4-pin header on the back (SWCLK, GND, SWDIO, RUN) takes a Raspberry Pi Debug
Probe. The game data goes in the flash with the program, or on a FAT32 microSD card.

## Modules plugged in by hand

- **DS1: 2.0" 240 × 320 ST7789 IPS SPI module** (MSP2008 pinout, 8 pins: GND, VCC, SCL, SDA, RES,
  DC, CS, BLK) into the 1x8 socket JLCPCB solders on the front.
- An **8 Ω speaker** (up to 3 W) with a JST PH 2-pin plug into J5 at the right edge of the back.
- Two **18650 cells**, and a **microSD card** if the WAD lives there.

## Firmware: build

- Pico SDK 2.x with a board header for this layout (W25Q128 16 MB; the PSRAM on QMI M1 with chip
  select GPIO8 is configured by the firmware — the boot ROM only sets up CS0), `-DPICO_PLATFORM=
  rp2350-riscv` (or `rp2350-arm-s`). The program is not written yet (see the repository's README;
  `rp2350-handheld`'s `doom/` is the closest start).

## Bill of materials

Rough cost at 10 pieces (estimates): RP2350A $1.10 · W25Q128 $1.00 · APS6404L $1.20 · crystal $0.15 ·
3.3 µH inductor $0.20 · ST7789 2.0" $4.00 · microSD socket $0.60 · MAX98357A $0.90 · BQ24075 $1.10 ·
DW01A + FS8205A + AO3401A $0.25 · MAX17048 $1.60 · TLV75533 $0.20 · USB-C + ESD + TVS + PTC $0.50 ·
buttons $3.50 · passives $2.00 · two 18650 holders $9.80 · 4-layer PCB $4.00 · speaker $1.00 →
**about $37** without the cells, plus assembly.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 1 | C1 | 10uF | C19702 | C_0603_1608Metric |
| 2 | C2, C3 | 15pF | C1548 | C_0402_1005Metric |
| 16 | C4, C5, C6, C7, C8, C9, C10, C11, C12, C13, C14, C18, C19, C21, C23, C27 | 100nF | C1525 | C_0402_1005Metric |
| 5 | C15, C16, C17, cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 1 | C20 | 1uF | C52923 | C_0402_1005Metric |
| 1 | C22 | 10uF | C15850 | C_0805_2012Metric |
| 3 | C28, C29, cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | D1 | tvs 5V | C918844 | D_SOD-323 |
| 1 | DS1 | Female header 1x8 2.54 mm | C2905417 | TFT_2.0in_ST7789_Socket_1x08_P2.54mm |
| 1 | F1 | PTC 1000mA | C20800 | Fuse_1206_3216Metric |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P | C165948 | USB_C_Receptacle_USB2.0_16P |
| 1 | J3 | PinHeader_1x04_P2.54mm_Vertical | C2691448 | PinHeader_1x04_P2.54mm_Vertical |
| 1 | J4 | DM3AT-SF-PEJM5 | C114218 | Hirose_DM3AT_microSD_socket_push-push |
| 1 | J5 | S2B-PH-K-S(LF)(SN) | C173752 | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | L1 | 3.3uH | C83456 | L_Power_6x6mm |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 7 | R1, R2, R3, R4, R5, R12, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 2 | R6, R16 | 100kohm | C25741 | R_0402_1005Metric |
| 3 | R7, R11, R18 | 1kohm | C11702 | R_0402_1005Metric |
| 1 | R8 | 33ohm | C25105 | R_0402_1005Metric |
| 2 | R9, R10 | 27ohm | C25100 | R_0402_1005Metric |
| 1 | R13 | 634kohm | C270588 | R_0402_1005Metric |
| 2 | R14, R15 | 5.1kohm | C25905 | R_0402_1005Metric |
| 1 | R17 | 100ohm | C25076 | R_0402_1005Metric |
| 2 | R21, R22 | 3.3kohm | C25890 | R_0402_1005Metric |
| 12 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8, SW9, SW10, SW11, reset | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW12 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 1 | U1 | RP2350A | C42411118 | QFN-60-1EP_7x7mm_P0.4mm_EP3.4x3.4mm |
| 1 | U2 | W25Q128JVSIQ | C97521 | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U3 | APS6404L-3SQR-SN | C5333729 | SOIC-8_3.9x4.9mm_P1.27mm |
| 1 | U4 | TLV75533PDBVR | C404027 | SOT-23-5 |
| 1 | U5 | MAX98357AETE+T | C910544 | QFN-16-1EP_3x3mm_P0.5mm_EP1.1x1.1mm |
| 1 | U6 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U7 | MAX17048G+T10 | C2682616 | DFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.2mm |
| 1 | U8 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U9 | ESD 2-line | C7519 | SOT-23-6 |
| 1 | Y1 | 12MHz 10pF | C20625731 | Crystal_SMD_3225-4Pin |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

The RP2350 SoC being written now (Hazard3, Xh3irq, TIMER, IO_BANK0, SIO, RESETS), plus: the **QMI
with both chip selects** — CS0 set up by the boot ROM, **CS1 configured by the firmware** (M1 timing
and read/write formats, GPIO8 funcsel XIP_CS1n) — in front of **two board memory components** on one
shared QSPI bus (W25Q128, APS6404L); the XIP cache including writes to PSRAM; the boot ROM's
IMAGE_DEF search and BOOTSEL entry by QSPI_SS low; PL022 SPI0/SPI1 with DMA; PIO for I2S; I2C1
(DesignWare) to the MAX17048 model; the core regulator as a fixed 1.1 V; and the board's BQ24075 /
two-cell / protection / LDO chain.

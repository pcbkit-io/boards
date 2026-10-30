# 05 · f411-psram — an STM32 "Black Pill" chip with 8 MB of SPI PSRAM

The STM32F411CEU6 (the chip of WeAct's Black Pill) is a fast Cortex-M4F with only 128 KB of SRAM and
no memory-mapped QSPI, so the board adds an 8 MB APS6404L PSRAM and a 16 MB W25Q128 on SPI1 and the
port pages Doom's heap through a software cache. A 2.0" IPS, an I2S amplifier, eight buttons, and two
18650 cells it charges from its USB-C. Firmware goes in through the same USB-C by the ROM's USB DFU
bootloader, or SWD. JLCPCB assembles everything but the display module, which plugs into the socket
it solders.

| | |
|---|---|
| Processor | STM32F411CEU6 (UFQFPN-48): Arm **Cortex-M4F** (ARMv7E-M, Thumb-2, DSP, single-precision FPU), 100 MHz, 128 KB SRAM, 512 KB flash with ART accelerator; 25 MHz HSE crystal |
| Memory | APS6404L-3SQR 8 MB PSRAM (CS PA4) and W25Q128JV 16 MB flash (CS PB2) on SPI1 at 50 MHz (PA5 SCK, PA6 MISO, PA7 MOSI) |
| Display | 2.0" 240 × 320 IPS, ST7789 (MSP2008), SPI2 at 25 MHz (PB13 SCK, PB15 MOSI, PB12 CS, PB14 DC, PB1 RES), backlight PWM on PB10 (TIM2_CH3); used landscape 320 × 240 |
| Input | 8 × 6 mm tact switches: up PA1, down PA2, left PA3, right PB4, fire PB6, use PB7, strafe PB8, menu PB0 (internal pull-ups) |
| Sound | I2S3 (PB3 CK, PA15 WS, PB5 SD) → MAX98357A (mono mix, 9 dB) → 8 Ω speaker |
| Power | 2 × 18650 Li-ion in parallel (1S2P, a 2 A resettable fuse per cell) in Keystone 1042 holders → reverse-cell P-FET (AO3401A) → DW01A + FS8205A; BQ24075 charger with power path from USB-C; SYS → TLV75533 LDO (3.3 V, switched by its EN) and the amplifier |
| Programming | USB-C to PA11/PA12: hold BOOT (BOOT0 → 3.3 V) and press RESET → ROM DFU (`dfu-util`); SWD header J1 on the back (VCC, SWDIO, SWCLK, NRST, GND) for an ST-LINK |
| Board | 140 × 104 mm, **4 layers**, four M3 holes; front: display, buttons, BOOT, RESET, on/off switch, USB-C (top edge), charger, speaker connector (right edge), test pads; back: MCU, memories, amplifier, regulator, SWD header, the two cells |

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 3 warnings (erc-rail-name), 0 unrouted nets. 81 components, 62 nets, 759 track segments (2408 mm), 86 vias.
`analysis.run { lens: "ready" }`: Typical scenario, partly by family estimates. Nothing to fix, 2 to consider, 17 ok: weigh the warnings (tell the person about the ones that are their choice), then the board is ready to order.
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code.
<!-- /STATUS -->

Warnings left, and why they are acceptable:

| Warning | Why |
|---|---|
| `diff-pair-uncoupled`, `skew` on DP / DN | USB full speed (12 Mbit/s, used only by the ROM's DFU): the connector is on the front's top edge, the processor on the back; tens of mm uncoupled and a few mm of skew are harmless at this rate |
| J1 is not on an edge | the SWD header, for a programmer's clip during development: not a cable that stays |
| heat sources need exact thermal parameters; chips estimated by their family | the thermal estimate's own caveat: nothing on this board dissipates more than a few hundred mW |

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/doom/f411-psram, open the
Laboratory and press **Build & run**: the page builds both programs — `emu/` for the STM32F411
(Cortex-M4F) and `doom/` for the computer it emulates (pcbkit-virt, with `DOOM1.WAD` linked in) —
puts the second in the W25Q128 and runs the board.

## How Doom fits

The F411 has 128 KB of SRAM and no memory-mapped QSPI, so its firmware (`emu/`, 19.5 KB) is a
RISC-V computer built from the serial chips, as on doom-01: an RV32IMAC interpreter
(`emu/rvemu.c`) with a 64 KB direct-mapped write-back cache (2048 lines of 32 bytes) in its SRAM
over the APS6404L (the guest's 8 MB of RAM) and the W25Q128 (the guest, `doom/`: Doom for
pcbkit-virt with the WAD linked in, its code and constants read where they lie). A miss is one
SPI transaction at 50 MHz with fast read (0Bh): 5.9 µs with the chip selected, inside the
PSRAM's 8 µs limit. Each guest frame (320 × 200, 8-bit) goes to the ST7789 through the guest's
colour table as RGB565, on SPI2 at 25 MHz. Measured in PCBKit's simulator: the title screen at
about 40 s of device time, in full colour; then the demo, a few frames a second.

## Power

Two 18650 Li-ion cells in Keystone 1042 holders on the back, in parallel (1S2P, 3.0–4.2 V). From the
cells to the board:

- **Reverse cells:** an AO3401A P-MOSFET in the plus line, its gate at the cells' minus: cells put
  in backwards leave it off and the board unpowered (it conducts both ways when they are right, so
  charging goes through it too). One cell reversed against the other is what the **per-cell fuses**
  are for: each holder's + reaches the pack through its own 2 A resettable fuse (Littelfuse
  1812L200/16GR: 16 V, 100 A interrupting), so a cell put in backwards, or one much fuller than
  the other, trips both in milliseconds instead of shorting the two cells together (tens of
  amperes); take the cell out and they reset. Still put each cell's + at the end the silkscreen
  marks.
- **Protection:** DW01A + FS8205A in the pack's minus: over-charge 4.3 V, over-discharge 2.4 V,
  over-current about 3 A and short circuit, for the pack.
- **Charger with power path:** a TI BQ24075 from the USB-C's VBUS (5.1 kΩ CC pull-downs: any USB-C
  charger gives 5 V), in USB500 mode (at most 500 mA from the port), 800 mA fast charge (RISET
  1.13 kΩ), input limit by RILIM 1.18 kΩ, safety timers off (two cells charge longer than its
  longest timer; termination is by current), TS tied to 10 kΩ (no thermistor). Its output SYS runs
  the board from USB when one is plugged in and from the cells otherwise.
- **Rails:** SYS feeds the TLV75533 LDO (3.3 V for the MCU, memories and display) and the
  MAX98357A directly. Near empty (3.0–3.3 V cells) the LDO drops out and 3.3 V follows the cells
  down; the F411, the memories and the ST7789 run to 2.7 V.
- **On/off:** the slide switch on the left edge drives the LDO's enable (off: the 3.3 V rail is off
  and the amplifier is shut down by its SD_MODE pull-up; the charger still charges).

| State | Current (estimate) |
|---|---|
| STM32F411 at 100 MHz | ~25 mA |
| PSRAM + flash active | ~15 mA |
| ST7789 2.0" with backlight | ~40 mA |
| MAX98357A playing | 20–200 mA from SYS |
| **Typical** | **~110 mA at 3.7 V** |

Two 3000 mAh cells → **~50 h**. A full charge of two empty cells at 800 mA takes about 8 h.

## Charging

Plug any USB-C charger or computer into the USB-C on the top edge. The red LED (CHG) is on while the
cells charge and goes out when they are full; the green LED (USB) is on while USB power is present.
The board runs from USB while it charges, switched on or off. Insert the cells with + at the end the
back's silkscreen marks.

## Modules plugged in by hand

The assembly solders a 1x8 female socket; the module is bought separately and plugged in:

- **DS1: 2.0" 240 × 320 ST7789 IPS SPI module** (MSP2008-type, 8 pins: GND, VCC, SCL, SDA, RES, DC,
  CS, BLK), on the front.
- An 8 Ω speaker (1–3 W) on the JST PH 2-pin connector J5 at the right edge.

## Firmware: build and flash

- **Build:** in PCBKit, the Laboratory's **Build & run** builds both programs (the F411's for
  Cortex-M4F with hard float; the guest for RV32IMAC with the WAD); then export the firmware
  (`export.run { format: "firmware" }`): `U1.bin` for the STM32F411 and `U3.bin` for the W25Q128.
- **The STM32F411, on arrival:** switch the board on, plug its USB-C into a computer, hold BOOT and
  press and release RESET (then release BOOT): the F411's ROM boot loader enumerates as "STM32
  BOOTLOADER" (USB DFU, 0483:df11). Then `dfu-util -a 0 -s 0x08000000:leave -D U1.bin` (or
  STM32CubeProgrammer). The ROM's DFU needs the 25 MHz HSE, which is fitted. Or over SWD: an
  ST-LINK on J1 (VCC, SWDIO, SWCLK, NRST, GND), `st-flash write U1.bin 0x8000000`.
- **The W25Q128 (the guest):** hold MENU while switching on: the firmware lets go of the memory bus
  and waits, so a USB SPI programmer (CH341A) with a SOIC-8 clip on the W25Q128 writes it in
  place: `flashrom -p ch341a_spi -w U3.bin`. Switch off and on: Doom boots.

## Bill of materials

Rough cost at 10 pieces (estimates, JLCPCB prices): STM32F411CEU6 $5.30 · APS6404L $4.00 · W25Q128
$2.60 · 25 MHz crystal $0.10 · ST7789 2.0" module $4.00 · MAX98357A $1.00 · BQ24075 $1.10 · DW01A +
FS8205A + AO3401A $0.25 · TLV75533 $0.15 · two Keystone 1042 $9.80 · USB-C $0.20 · buttons $2.50 ·
passives $1.00 · 4-layer PCB $3.50 → **about $35** without the cells and assembly fees.

<!-- BOM -->
The assembly BOM as `pcbkit export --format fab` writes it (JLCPCB assembly: LCSC codes; the modules' lines are their sockets):

| Qty | Refs | Part | LCSC | Footprint |
|---|---|---|---|---|
| 2 | BT1, BT2 | 1042 | C5249310 | BatteryHolder_Keystone_1042_1x18650 |
| 2 | C1, C2 | 15pF | C1548 | C_0402_1005Metric |
| 9 | C3, C4, C5, C8, C11, C13, C14, C16, C19 | 100nF | C1525 | C_0402_1005Metric |
| 4 | C6, C7, cBat, cSys | 4.7uF | C1779 | C_0805_2012Metric |
| 2 | C10, C12 | 1uF | C52923 | C_0402_1005Metric |
| 1 | C15 | 10uF | C15850 | C_0805_2012Metric |
| 3 | C20, C21, cIn | 1uF | C15849 | C_0603_1608Metric |
| 1 | D1 | tvs 5V | C502527 | D_SOD-323 |
| 1 | DS1 | MSP2008 2.0in 240x320 IPS SPI module (ST7789) | C2905417 | TFT_2.0in_ST7789_Socket_1x08_P2.54mm |
| 2 | F1, F2 | PTC 2000mA | C18198340 | Fuse_1812_4532Metric |
| 1 | J1 | PinHeader_1x05_P2.54mm_Vertical | C492404 | PinHeader_1x05_P2.54mm_Vertical |
| 1 | J2 | USB4105-GF-A | C165948 | USB_C_Receptacle_USB2.0_16P |
| 1 | J5 | S2B-PH-K-S(LF)(SN) | C173752 | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | Q1 | AO3401A | C15127 | SOT-23 |
| 1 | Q2 | FS8205A | C5148694 | TSSOP-8_4.4x2.93mm_P0.65mm |
| 7 | R1, R3, R4, R5, R6, R7, rTs | 10kohm | C25744 | R_0402_1005Metric |
| 2 | R2, R11 | 100kohm | C25741 | R_0402_1005Metric |
| 1 | R8 | 634kohm | C270588 | R_0402_1005Metric |
| 2 | R9, R10 | 5.1kohm | C25905 | R_0402_1005Metric |
| 1 | R14 | 100ohm | C25076 | R_0402_1005Metric |
| 1 | R15 | 1kohm | C11702 | R_0402_1005Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | PTS645SM43SMTR92 LFS | C221880 | SW_Tactile_SPST_6x6mm_SMD |
| 1 | SW9 | MSK12C02 | C431540 | SW_Slide_SPDT_Shouhan_MSK12C02 |
| 2 | SW10, SW11 | TS-1187A-B-A-B | C318884 | SW_Tactile_SPST_5.1x5.1mm_SMD |
| 1 | U1 | STM32F411CEU6 | C60420 | QFN-48-1EP_7x7mm_P0.5mm_EP5.6x5.6mm |
| 1 | U2 | APS6404L-3SQR-SN | C5333729 | SOIC-8_3.9x4.9mm_P1.27mm |
| 1 | U3 | W25Q128JVSIQ | C97521 | SOIC-8_5.23x5.28mm_P1.27mm |
| 1 | U4 | TLV75533PDBVR | C404027 | SOT-23-5 |
| 1 | U5 | MAX98357AETE+T | C910544 | QFN-16-1EP_3x3mm_P0.5mm_EP1.1x1.1mm |
| 1 | U6 | BQ24075RGTR | C15464 | QFN-16-1EP_3x3mm_P0.5mm_EP1.68x1.68mm |
| 1 | U7 | DW01A | C351410 | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U8 | USBLC6-2SC6 | C7519 | SOT-23-6 |
| 1 | Y1 | 25MHz 10pF | C70582 | Crystal_SMD_3225-4Pin |
| 1 | ledChg | red | C2286 | LED_0603_1608Metric |
| 1 | ledPg | green | C12624 | LED_0603_1608Metric |
| 2 | rChg, rPg | 1.5kohm | C25867 | R_0402_1005Metric |
| 1 | rIlim | 1.18kohm | C270651 | R_0402_1005Metric |
| 1 | rIset | 1.13kohm | C67014 | R_0402_1005Metric |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** **ARMv7E-M** = the ARMv6-M core extended with Thumb-2 (32-bit encodings, IT blocks,
  bit-field, saturating and DSP instructions, hardware divide) and the **FPv4-SP FPU** (S0–S31, FPSCR,
  lazy stacking on exception entry). Exception model as v6-M plus priority grouping, BASEPRI, fault
  status registers.
- **Interrupt controller:** NVIC with 86 lines, SysTick, SCB (VTOR).
- **Boot:** BOOT0 strap: flash at 0x0800_0000 aliased to 0x0, or the **system memory ROM** (USART/USB
  DFU bootloader, ST's code: modelled by behaviour, not by its image); image format: a raw binary with
  the vector table first.
- **Clocks (RCC):** HSE 25 MHz → PLL (M/N/P/Q) → 100 MHz SYSCLK, 48 MHz for USB; flash wait states
  (FLASH_ACR) that the firmware sets before raising the clock.
- **Peripherals:** GPIO with AFR **pin-mux tables** (AF5 SPI1/2, AF6 I2S3, AF1 TIM2) — data from ST's
  open pin XML/SVD; SPI1/SPI2 with **DMA2/DMA1 streams**; I2S3 with its PLLI2S; TIM2 PWM; USB OTG FS
  (device, DFU class) if flashing through USB is simulated; SysTick.
- **Board:** APS6404L and W25Q128 models behind chip selects (the link needs CS), ST7789 model,
  MAX98357A as an I2S sink to WebAudio, two 18650 cells with DW01A thresholds and BQ24075 CC/CV
  charge behind its power path.

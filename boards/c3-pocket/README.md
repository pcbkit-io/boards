# 03 · c3-pocket — a Game Boy-shaped RISC-V Doom on an ESP32-C3

A small portrait handheld (52 × 92 mm) around an ESP32-C3-MINI-1 module: one RISC-V core, 400 KB of
SRAM, 4 MB of flash, a 1.3" IPS screen, eight buttons, a piezo and a protected LiPo with a charger.
The C3 has no PSRAM, so Doom is cut down the way rp2040-doom cut it for the RP2040.

| | |
|---|---|
| Processor | ESP32-C3-MINI-1-N4: ESP32-C3, one **RV32IMC** core (Espressif's own, 4-stage pipeline) at 160 MHz, 400 KB SRAM (16 KB of it cache), 384 KB ROM, 4 MB flash in the module (MSPI, XIP through the cache), Wi-Fi/BLE unused |
| Display | 1.3" 240 × 240 IPS, ST7789, SPI without CS (mode 3): GPIO6 SCK, GPIO7 MOSI, GPIO5 D/C, GPIO4 RES; backlight on |
| Input | 8 × 5.1 mm tact switches: d-pad IO0–IO3, fire IO10, use RXD0 (IO20), strafe TXD0 (IO21), menu IO9 (= BOOT: held at reset it enters download mode) |
| Sound | piezo on IO8 (LEDC PWM; IO8 is a strapping pin held high by 10 kΩ, the piezo is DC-open) |
| Power | 1-cell LiPo (JST PH) → DW01A + FS8205A protection; MCP73831 charger (500 mA, from USB-C VBUS); AP2112K-3.3 LDO (600 mA) enabled by the slide switch |
| Programming | USB-C to the C3's own USB Serial/JTAG (IO18/IO19): `esptool.py` flashes it, no bridge chip; JTAG debugging over the same cable |
| Board | 52 × 92 mm, 2 layers; front: display, buttons, switch, USB-C; back: module (top edge, antenna away from the hand), power, piezo |

<!-- STATUS -->
**Board status** (`pcbkit check`, measured on the dev host): **complete** — 0 errors, 1 warnings, 0 unrouted nets; 40 components, 33 nets, 375 track segments (1028 mm), 46 vias.

| Warning | Why it is acceptable |
|---|---|
| `diff-pair-uncoupled`: pair DP / DN: 22.36 mm uncoupled (61.09 mm coupled), over the pair's default's 10 mm | USB full speed (12 Mbit/s): the connector is on the front edge, the processor on the back; tens of mm uncoupled are harmless at this rate |

<!-- /STATUS -->

## How Doom fits

400 KB SRAM with ~320 KB free after the radio-less IDF runtime: less than Doom's usual 4–8 MB, but
more than the RP2040's 264 KB, so the rp2040-doom recipe applies: the WAD in a WHD-style compressed
form memory-mapped from flash (the 4 MB flash holds code + the shareware episode), constant tables
left in flash, a small zone heap, one 8-bit 240 × 150 frame, sent to the ST7789 as RGB565 lines by
SPI DMA at 80 MHz (one 240 × 240 frame is 115 KB: ~12 ms). Estimate: 20–30 fps at 160 MHz (no port
measured here; Doom ports exist for the ESP32-C3 but their numbers are not verified).

## Power

| State | Current (estimate) |
|---|---|
| ESP32-C3 at 160 MHz, radio off | 25–30 mA |
| ST7789 1.3" with backlight | 20–30 mA |
| **Total** | **~55 mA at 3.3 V** |

A 1000 mAh LiPo (3.7 V nominal) → **~15 h**. Charging at 500 mA (RPROG 2 kΩ) takes ~2.5 h. The
AP2112K drops 250 mV at 600 mA, so the device runs until the cell is ~3.45 V (most of its capacity);
DW01A cuts the cell off at 2.4 V and on overcurrent.

## Firmware: build and flash

- ESP-IDF 5.x (`idf.py set-target esp32c3`), GCC `riscv32-esp-elf`, `-march=rv32imc_zicsr_zifencei`.
- The image format is Espressif's: a second-stage bootloader at 0x0, the partition table at 0x8000,
  the app at 0x10000 (segments with load addresses, a checksum and SHA-256), the WAD in a data
  partition mapped with `esp_partition_mmap`.
- Flash: `idf.py -p /dev/ttyACM0 flash` (or `esptool.py --chip esp32c3 write_flash 0x0 bootloader.bin
  0x8000 partitions.bin 0x10000 doom.bin 0x110000 doom1.whd`). If the app crashes before USB comes up,
  hold MENU (IO9) and press RESET: the ROM's download mode.

## Bill of materials

Rough cost at 10 pieces (estimates): ESP32-C3-MINI-1 $2.20 · ST7789 1.3" module $3.00 · MCP73831 $0.45
· DW01A + FS8205A $0.15 · AP2112K $0.15 · USB-C + ESD $0.40 · buttons $0.30 · piezo $0.30 · switch
$0.30 · 1000 mAh LiPo $4.00 · PCB $1.50 → **about $13**.

<!-- BOM -->
All components as the design places them (`parts.bom`; passives are specified by value and package, the orderable codes are chosen by `pcbkit parts`):

| Qty | Refs | Value | Part number | Footprint |
|---|---|---|---|---|
| 1 | BZ1 |  | GPC12075YB-5V4000 | Buzzer_D12.0mm_H7.5mm_P6.50mm |
| 1 | C1 | 10uF |  | C_0805_2012Metric |
| 2 | C2, C6 | 100nF |  | C_0402_1005Metric |
| 3 | C3, C7, C8 | 1uF |  | C_0402_1005Metric |
| 2 | C4, C5 | 4.7uF |  | C_0805_2012Metric |
| 1 | D1 | tvs 5V |  | D_SOD-323 |
| 1 | D2 | red |  | LED_0603_1608Metric |
| 1 | DS1 |  | MSP1308 1.3in 240x240 IPS SPI module (ST7789) | TFT_1.3in_ST7789_Socket_1x07_P2.54mm |
| 1 | J1 | USB_C_Receptacle_USB2.0_16P |  | USB_C_Receptacle_USB2.0_16P |
| 1 | J2 |  | S2B-PH-K-S(LF)(SN) | JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal |
| 1 | Q1 |  | FS8205A | TSSOP-8_4.4x2.93mm_P0.65mm |
| 1 | R1 | 100kohm |  | R_0402_1005Metric |
| 3 | R2, R3, R4 | 10kohm |  | R_0402_1005Metric |
| 2 | R5, R6 | 5.1kohm |  | R_0402_1005Metric |
| 1 | R7 | 2kohm |  | R_0402_1005Metric |
| 1 | R8 | 470ohm |  | R_0402_1005Metric |
| 1 | R9 | 100ohm |  | R_0402_1005Metric |
| 1 | R10 | 1kohm |  | R_0402_1005Metric |
| 8 | SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 | SW_Tactile_SPST_5.1x5.1mm_SMD |  | SW_Tactile_SPST_5.1x5.1mm_SMD |
| 1 | SW9 |  | JS102011SAQN | SW_Slide_SPDT_CK_JS102011SAQN_Horizontal |
| 1 | SW10 | SW_Tactile_SPST_6x6mm_SMD |  | SW_Tactile_SPST_6x6mm_SMD |
| 1 | U1 |  | ESP32-C3-MINI-1-N4X | ESP32-C3-MINI-1_module_13.2x16.6mm_LGA_53_pads |
| 1 | U2 |  | MCP73831T-2ACI/OT | SOT-23-5 |
| 1 | U3 |  | DW01A | SOT-6_1.6x2.9mm_P0.95mm |
| 1 | U4 |  | AP2112K-3.3TRG1 | SOT-23-5 |
| 1 | U5 | ESD 2-line |  | SOT-23-6 |
<!-- /BOM -->

## What PCBKit's engine must model to simulate it faithfully

- **Core:** RV32IMC (the existing RV32IMAC core without A; machine mode only), Espressif's custom
  CSRs (performance counters `mpcer`/`mpcmr`, the CPU interrupt matrix's mapping) and 32 interrupt
  lines routed by the **interrupt matrix** (any peripheral source to any CPU line).
- **Boot ROM:** the first-stage ROM loads the second-stage bootloader from flash by Espressif's image
  format (segments, checksum, hash), reads strapping pins (IO2, IO8, IO9) for download mode and runs
  the USB Serial/JTAG download protocol; a model can skip to the app image's entry like our direct
  XIP profile, but the image format (segments with load addresses) must be understood.
- **Memory:** the MSPI flash with the cache and **MMU** (64 KB pages mapping flash into the I/D
  address spaces: the WAD is read through it).
- **Clocks:** PLL 480 MHz → CPU 160 MHz, APB 80 MHz; SYSTIMER (two 52-bit counters) for FreeRTOS ticks.
- **Peripherals:** GPSPI2 with its DMA (GDMA), the **GPIO matrix / IO MUX** (any peripheral signal to
  any pin: the pin-mux table is the SoC description), LEDC, USB Serial/JTAG (as a serial port), TIMG
  watchdogs (must be fed or disabled), eFuse reads (MAC, chip revision) that the IDF does at start.
- **Board:** ST7789 controller model (CASET/RASET/RAMWR, MADCTL, RGB565), LiPo with charger and LDO.

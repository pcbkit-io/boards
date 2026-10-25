# can-node — an STM32G431 sensor node on a robot's CAN bus

A node for a robot or a machine: four 0–5 V pressure sensors and a 6-axis IMU, sent at 100 Hz as CAN
frames at 500 kbit/s. The bus and the robot's 12 V come in on one JST GH connector; the board
protects itself (reverse polarity, transients) and makes its own 3.3 V. A jumper puts the 120 Ω
terminator on the bus for the two nodes at its ends. Firmware is ST's HAL, flashed over SWD.

| | |
|---|---|
| Processor | STM32G431CBU6 (UFQFPN-48): Arm Cortex-M4F at 170 MHz (8 MHz crystal, PLL, boost mode), FDCAN1 |
| CAN | SN65HVD230 transceiver on PA12 (TX) / PA11 (RX), 500 kbit/s; SD24C TVS on CANH and CANL; 120 Ω terminator through jumper J2 |
| Inputs | J5 (2 × 4, 2.54 mm): four 0–5 V inputs, each with its ground, through an 11 k / 20 k divider and a 1 kHz filter into ADC1 IN1–IN4 (PA0–PA3), sampled by TIM6 at 100 Hz through DMA |
| IMU | ICM-42688-P at 0x68 on I2C1 (PA15 SCL, PB7 SDA) at 400 kHz, ±4 g and ±1000 °/s |
| Power | J1 (JST GH 4-pin: 12 V, CANH, CANL, GND) → AO3401A reverse-polarity P-FET → SMBJ18A TVS → TPS54331 buck → 3.3 V |
| Debug | J4: SWD (3.3 V, SWDIO, SWCLK, NRST, GND) for an ST-LINK; J3: the console (USART1 TX, RX, GND at 115200 baud) for a 3.3 V USB-serial cable; D1: status LED on PB0 |
| Board | 60 × 40 mm, 2 layers, two M3 holes |

Frames it sends (little-endian 16-bit values):

| ID | Data |
|---|---|
| 0x100 | the four inputs in mV at the connector (0–5000) |
| 0x101 | acceleration X, Y, Z in mg, the IMU's temperature in 0.01 °C |
| 0x102 | rotation X, Y, Z in 0.1 °/s |

A frame **0x200** with one byte sets the LED: 0 off, 1 on, 2 blinking.

<!-- STATUS -->
**Board status** (measured on the dev host with PCBKit's CLI): `pcbkit check`: complete — 0 errors, 0 warnings, 0 unrouted nets. 68 components, 37 nets, 494 track segments, 53 vias.
`analysis.run { lens: "ready" }`: nothing to fix, 11 to consider (below).
`pcbkit export --format fab` succeeds without `--force`: every assembled line of the BOM has an LCSC code (JLCPCB assembly).
<!-- /STATUS -->

What the readiness check asks to consider, and why it is fine here:

| Finding | Why |
|---|---|
| J2, J3, J4 are not on an edge | the terminator jumper, the console and the SWD header: set once or used on the bench, no cable stays |
| V3 comes in through J4 unprotected | the SWD header's 3.3 V is the programmer's reference, not a supply |
| D1 runs at 0.3 mA | a status LED; 1 kΩ keeps it from lighting the robot's inside |
| No test points | the console and SWD reach every signal that matters |
| U4 needs an operating load / thermal parameters / family estimates | the analysis' own caveats: the node draws about 120 mA |

## Run it in the browser

Open https://app.pcbkit.io/github/pcbkit-io/boards/tree/main/devices/can-node, open the Laboratory and
press **Build & run**: the page builds `firmware/` with ST's HAL for the STM32G431 and runs it on a
model of the chip. `sim/robot.js` is the robot around it: the gripper's two cylinders filling and
venting (supply pressure, cycle time, a leak on cylinder B, a bump of the robot are controls),
driving the four inputs and the IMU. What you see:

- **CAN_BUS1** — the bus beyond the connector as a USB-CAN adapter: every frame as an SLCAN line
  (`t1008…`); send `t2001` + `02` + CR to make the LED blink.
- **J3** — the console: `can-node 170000000 Hz`, `imu ICM-42688-P`, the inputs once a second.

The model refuses what silicon would not do (a peripheral with its clock off, a PLL outside its
limits) and says so; it does not model the bus's analog waveform or arbitration with other nodes.

## Order, flash, use

1. Order with assembly (JLCPCB: `pcbkit export --format fab`, or `fab.order` in the app).
2. Flash over SWD with an ST-LINK on J4: `pcbkit export --format firmware` gives `U1.bin` and
   `U1.hex`; `st-flash write U1.bin 0x08000000` or STM32CubeProgrammer.
3. Wire J1 to the robot's bus (12 V, CANH, CANL, GND); fit J2's shunt on the two end nodes only.

Licence: CC0 1.0.

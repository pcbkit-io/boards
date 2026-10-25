# Espressif boot ROMs

The mask ROMs of Espressif's chips, which PCBKit's simulation runs as the chip does from power-on
(the boot ROM loads the second-stage boot loader from flash, as on silicon). PCBKit's laboratory
fetches each by its address here and checks its SHA-256 before it runs.

| File | Chip | SHA-256 | Size |
|---|---|---|---|
| `esp32p4_rev0_rom.elf` | ESP32-P4 (chip revisions v0.x and v1.x) | `948f2c7d108d04a9c3a982f9c1c498fb17f2c4a68abc51f87782592034d7ce44` | 243 316 bytes |

Source: Espressif's [esp-rom-elfs](https://github.com/espressif/esp-rom-elfs), release
[20260528](https://github.com/espressif/esp-rom-elfs/releases/tag/20260528), unchanged. Copyright
Espressif Systems (Shanghai) Co., Ltd., under the Apache License 2.0 ([`LICENSE`](LICENSE)).

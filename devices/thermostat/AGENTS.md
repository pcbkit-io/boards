# Circuit board (PCBKit)

This repository is a KiCad project with a `.pcbkit/` folder. KiCad's files hold the board as built;
`.pcbkit/design.pcbkit` holds the design in PCBKit, a small language with units, typed nets and
pins, parts and blocks. **You edit `.pcbkit/design.pcbkit` and run `pcbkit`; you never edit KiCad's
files by hand.** `.pcbkit/FORMAT.md` is the full reference.

## Files
| File | Who writes it |
|---|---|
| `.pcbkit/design.pcbkit` | **you**: nets, parts, components and their connections, blocks, placement rules, pours, classes, rules |
| `.pcbkit/blocks/`, `.pcbkit/parts/`, `.pcbkit/footprints.pcbkit` | you and the `parts.*` commands: modules the design uses (`use "blocks/x.pcbkit" as x`) |
| `*.kicad_pcb`, `*.kicad_pro`, `*.kicad_sch` | the tools and KiCad: where every part is, the copper, the fills, the orderable part chosen for each (`pcbkit parts`); they follow every change of the design. Change them with `pcbkit` commands, not by hand |
| `.pcbkit/comments.pcbkit` | the person and agents: requests pinned to the board |

A KiCad part the design does not define is the KiCad board's; to change it, move it with
`board.move` or take it over by defining a component of the same refdes in the design.

## The loop
1. `pcbkit call comments.list '{"status": "open"}'`: what the person asked. Answer each with
   `comments.reply` (`"status": "resolved"`, or `"dismissed"` with the reason).
2. Edit `.pcbkit/design.pcbkit`.
3. `pcbkit check`: `error` → fix `errors` (each with file, line, column); `incomplete` → step 4;
   `complete` → done.
4. `pcbkit route`: places the parts without `at`, routes, refills the pours (into the KiCad board).
5. `git commit`: the hook runs `pcbkit check` and refuses a board with errors.
6. `pcbkit export`: the fabrication zip (Gerbers, drill, BOM, placement) into `fabrication/`.
7. To order: `pcbkit call fab.shops` (the fabs, estimates, what each cannot make), then
   `pcbkit call fab.order '{"shop":"pcbway"}'`: the fab's order page with the board on it (`url`).

Every command prints one JSON document with a `summary` (what happened, the next step); exit 0
fine, 1 problems, 2 could not run.

## Writing the design
```
pcbkit 1
net VBUS { current = 500mA }                        // its peak current: the router widens its tracks
net V3, GND, DP, DN, TX, RX

usb = UsbC2(vbus = VBUS, gnd = GND, dp = DP, dn = DN) {}   // a block: sizes and places its parts
ldo = Ldo(vin = VBUS, vout = V3, gnd = GND, current = 200mA) {}
uart = UsbUart(dp = DP, dn = DN, vcc = V3, gnd = GND, tx = TX, rx = RX) {}
J1 = header(4) {
  place = [edge(.bottom)]                            // intent, not coordinates
  pins = [V3, GND, TX, RX]
}
C1 = cap(10uF, package = "0805") {
  place = [near(J1, max = 3mm)]
  pins { a: V3, b: GND }
}
ground = pour(net = GND, layers = [Cu.bottom]) {}
```
- Units are always written (`0.25mm`, `90deg`, `100nF`); points are `(10mm, 5mm)`, origin
  lower-left, Y up.
- Leave `at` out: the engine places the part. Give `at` only to pin a part where it must be.
- `pins` keys are logical pins (`SDA: SDA`) or pad numbers (`"25": GND`); `.nc` = deliberately open.
- Nets of the KiCad board are yours by name: `net GND`, `net V3 named "+3V3"`.
- Power tracks: state the net's peak `current` (`net VIN { voltage = 12V; current = 2A }`: a
  supply from a connector); the router widens its tracks for it. Net classes are for clearance and
  vias (`hv = NetClass { clearance = 0.5mm; nets = [VIN] }`).
- State what a net is only where its pins do not: a net's role, voltage and current come from its
  typed pins and blocks, else its name (`pcbkit show` says each and why); `net SHIELD { role =
  .ground }`. A differential pair: `pair USB = (DP, DN) { impedance = 90ohm }` (`UsbC2` declares
  its own).
- A typo is an error with a suggestion; `pcbkit check` names file, line and column.

## Find before you write
- `lang.search`, `lang.describe`: every function, type and circuit block of the standard library (`lang.search { kind: "block" }`: regulators, USB, MCU support, I2C, LEDs, buttons, mounting).
- `parts.search` → `parts.describe` → `parts.use`: real parts with their pin tables and datasheet
  support circuits. Never invent a pin table; for a part nobody has, `parts.sheet` explains how to
  read its datasheet into a sheet, then `parts.create { dry_run: true }` and `parts.create`.
- `boards.search`, `boards.get`: open boards by their authors to learn from (keep their licence
  when you copy).

Run any command with `pcbkit call <name> '<json>'`; `pcbkit commands` lists them all. What a
command acts on is `id` or `ids` (`"R1"` or `"comp:R1"`); a quantity is safest as a string with
its unit (`"0.2mm"`, `"90deg"`; a bare number is in the unit its schema names); lists come in
pages (`offset`, `limit`). `pcbkit call model.get '{"ids": ["R1"]}'` describes a component.

## Showing the person
`pcbkit view --open` opens the web app on this repository: edits appear live both ways;
`view.select`, `view.camera`, `view.highlight` show what you talk about (`{ "ids": ["U1"] }`). `pcbkit render --out
board.png` (`--3d`) draws the board. `share.link` gives a link anyone can open.

## Firmware and simulation
Start with `pcbkit call help '{"topic": "simulation"}'`. Never claim code ran or invent readings.

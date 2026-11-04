# The board format

A PCBKit project is its `.pcbkit/` folder, every file in the PCBKit language: `design.pcbkit`
holds what you and your agent write — the circuit in code, its intent, its rules — and
`board.pcbkit` the board as the tools built it (where every part is, the copper). KiCad and the
other EDA tools are an import and an export: `pcbkit import` turns their boards into these files,
`project.export { format: "kicad" }` writes a KiCad project from them. This document is the reference for
those files; the language (syntax, types, units, evaluation) is
[`language.md`](language.md), and the standard library — every type, field, block and footprint,
with a doc comment on each — is PCBKit too (`help.search`, `help.describe`; `pcbkit lsp` in an
editor).

Every example in a ```` ```pcbkit ```` block that starts with `// file:` is a complete file and is
evaluated by the tests (`crates/pcbkit-core/tests/format_doc.rs`), so the examples here always work.

## 1. Files of a project

| File | Written by | Holds |
|---|---|---|
| `.pcbkit/design.pcbkit` | people, agents | the design: nets with their voltages and currents, parts by what they must be, components and their connections, blocks, placement rules, pours, net classes, rules, the stack, the outline, routing settings |
| `.pcbkit/board.pcbkit` | the tools | the board as built (below): where each component is, the nets on its pads, the orderable part chosen for it, the tracks, vias and fixed copper. The router, the placer, `pcbkit parts`, `board.*` and every build write it |
| `.pcbkit/blocks/*.pcbkit`, `.pcbkit/parts/` | people, agents, `parts.create` | modules the design uses (`use ./blocks/power as power`; paths are relative to the file that uses them) |
| `.pcbkit/footprints.pcbkit` | `parts.use` (a library footprint), people, agents | the project's own footprints (`use ./footprints as fp`) |
| `.pcbkit/comments.pcbkit` | the web app, agents (`project.comment`, `project.reply`) | comments on the board, their threads and status (below) |
| `.pcbkit/orders/<digest>/` | `project.order` | one record per Gerber archive handed to a fab (`record.json`, `order@1`) and that archive itself, byte for byte: the parts it orders, their certificates, the tool's versions, every hand-over with the fab's reading. `project.order { reorder: "<digest>" }` sends that archive again |
| `.pcbkit/certificates/*.json` | certificate producers, certcheck (`verdicts.json`) | the ordered parts' certificates (`part@1`) and certcheck's verdicts of them: a refuted part stops `project.order` |
| `.pcbkit/FORMAT.md`, `AGENTS.md`, `CLAUDE.md`, `.githooks/pre-commit` | `init` | this document; how an agent works here; the hook that runs `pcbkit check` before a commit |

`.pcbkit/cache/` is the tools' cache (git-ignored): the pours' fills, computed from the copper and
the rules when the project is read and kept by the hash of what they depend on (fills are never
in git). There are no other generated files.

**What lives where.** Every fact has one home:

- **What the board is is the design's.** Every part, net, pour, class, rule, the stack and the
  outline are stated in `design.pcbkit` (and the modules it uses); `board.pcbkit` holds no intent.
- **Where things are is `board.pcbkit`'s.** Positions, the copper and the nets on the pads as
  built are physical: the tools write them there. A component the design gives `at` is pinned
  there; without `at` it stays wherever the placer or `board.move` put it. `board.move` of a part
  without `at` moves it in `board.pcbkit` and locks it there (`locked = true`): every later
  placement keeps a locked part, the engine's parts it stands on move instead (`place --all` too);
  `board.pin` writes the positions into the design as `at`. A component `board.pcbkit` does not
  name is unplaced: the next build places it.
- **`board.pcbkit` is the tools' file.** `design.write` and `design.edit` refuse it unless given
  `generated: true`; change the board with `board.*`, builds and routing. A hand edit is read like
  any change.

### The board as built (`.pcbkit/board.pcbkit`)

The file is PCBKit's data subset (literals only, read without evaluation, like
`comments.pcbkit`). Lengths are in mm and angles in degrees, without units:

```text
pcbkit 2
// The board as built: written by PCBKit's tools (ADR 0118). Lengths in mm, angles in degrees.
layers = 4
components = {
  R1 = { at = (10.5, 3), rotation = 90, side = .bottom, path = "usb/R", footprint = "R_0603", nets = { "1" = "GND", "2" = "VBUS" } },
}
tracks = [
  { net = "GND", layer = "F.Cu", width = 0.25, points = [(1, 2), (3, 4)] },
]
vias = [
  { net = "GND", at = (3, 4), diameter = 0.6, drill = 0.3 },
]
```

- `components`, by refdes: `at`, `rotation`, `side`, `locked`; `path`, the component's entity
  path in the design where it is not the refdes; its `footprint` name; the `nets` of its pads as
  built (the router's pin assignment included); the `fields` the parts choice wrote (`LCSC Part
  #`, `MPN`, `Manufacturer`, `pcbkit-need`).
- `layers = N` where the design leaves `layers = .auto`; `texts`: the positions of the board texts
  the engine placed.
- `tracks` (`net`, `layer`, `width`, `points`, `arc`: the middle point of an arc), `vias` (`net`,
  `at`, `diameter`, `drill`, `span`, `micro`, `unusedLayers`), `copper` (fixed copper polygons).
  Copper a person made (an imported board's) says `by = .person`: a build completes the routing
  around it and never redoes it; copper a person locked says `locked = true`: a change of the
  parts never rips it up.
- `from = "kicad"` on a board imported from another tool.

**A KiCad board** becomes a project: `pcbkit import x.kicad_pcb` (`project.import`, a drop or
Import in the web app; `pcbkit init` in a directory holding a KiCad board) writes it as the
project's files — its components on their own footprints in `.pcbkit/footprints.pcbkit`, its
nets, classes, rules, stack, outline, pours, keepouts and art in `design.pcbkit`, the positions
and copper in `board.pcbkit` (`by = .person`, `from = "kicad"`). The answer lists what the files
leave out (`left_out`: drawings on user layers, dimensions, groups, the title block, …). Edits
made in KiCad afterwards come back only through another import. `project.export { format:
"kicad" }` writes a KiCad project from the model (KiCad 8's format; KiCad 10's where footprints
have jumper pads). A directory of KiCad files without `.pcbkit/design.pcbkit` (a repository
`pcbkit check` or the CI gate runs on, a GitHub repository the app opens) is imported in memory
for that run and checks and renders as KiCad has it; the first change written makes it a project.

**A KiCad schematic** becomes the design: `pcbkit import` of a project's `.kicad_pro` or root
`.kicad_sch` (`project.import { format: "kicad-schematic" }`) writes the circuit its sheets draw
into `.pcbkit/design.pcbkit` — each symbol a component of its part (a resistor or capacitor of a
value as `res`/`cap` with its package, tolerance, rating, MPN and order code; any other symbol a
`part X: Part { … }` with its pins by name and their electrical types), nets under the schematic's names
(the board's, over a board), not-fitted parts `fitted = false` — and the footprints it names
into `.pcbkit/footprints.pcbkit`, as the board has them, else the project's libraries
(`fp-lib-table`), else KiCad's own. The design's netlist is KiCad's (`kicad-cli sch export
netlist`). With the project's board the board's parts keep where they are (`board.pcbkit`; those the
engine would move, an edge connector over the edge, are pinned with `at`); without one the
hierarchical sheets are blocks (a block per sheet file, called once per sheet; a file used by
several sheets takes its references as `refs`), the project's board setup, net classes (with
their nets) and custom rules are the design's `rules`, `NetClass`es and `interop`, and `pcbkit
route` places and routes a new board. `--relayout` (`relayout: true`) lays the project out anew
over its board's settings: the design states the board's outline, copper stack, thickness,
fabrication (copper weights, dielectric) and planes, pins its connectors, switches and mounting
holes where the board has them, and the build places and routes the rest.

**An Altium or EasyEDA schematic** becomes the design the same way: `pcbkit import` of an Altium
project's `.PrjPcb` (`project.import { format: "altium-schematic" }`: its SchDocs, over its
PcbDoc's footprints) or of an EasyEDA Standard `schematic-*.json` (`easyeda-schematic`: the
folder's sheets, over its one board), `pcbkit import x.epro --relayout` of an EasyEDA Pro project
(`easyeda-schematic` given the project: its board's schematic, over that board), or a drop of the
project's folder or zip onto the web app. Their sheets connect as their editors connect them
(wires, junctions, net labels, power ports, ports and sheet entries by the project's net scope,
buses and harnesses member by member, harnesses along their harness lines, Altium's pin maps and
repeated sheets as one channel each, `R1_CH1`).

**Boards of other tools** become a project the same way: `pcbkit import FILE` (or
`project.import`, or a drop onto the web app) reads an Eagle `.brd`, an Altium `.PcbDoc` (binary
or ASCII) or an EasyEDA board (Standard: its `.json`; Pro: its `.epro` project, the largest of its
boards) and writes it as the project's files, as a KiCad board above. The schematics beside the
board name its pads' pins (their names and electrical types): Eagle's `.sch` of the board's name, the SchDocs of the
Altium project (`.PrjPcb`) or folder, EasyEDA Standard's `schematic-*.json` beside its
`pcb-*.json` (OSHWLab's export of a project), an EasyEDA Pro project's own symbols. An EasyEDA
part's order codes become its component's `fields` in `board.pcbkit` (`LCSC Part #`, `MPN`, `Manufacturer`), from
the board or, where it has none, its schematic. Another router works on the board through Specctra: `pcbkit export
--format dsn` writes the design file it reads (Freerouting: `-de pcb.dsn -do pcb.ses`), and
`pcbkit import pcb.ses` puts its routing on this project's board. `pcbkit export --format
ipc2581` writes the whole board as one IPC-2581C file for fabs and assembly services that take it.

`pcbkit check` reads the project and prints one JSON report (`design.check`); `pcbkit show` describes
the board's objects (`board.get`); `pcbkit fmt` puts the sources in the canonical layout.

### Comments (`.pcbkit/comments.pcbkit`)

The person and agents leave comments on the board: a note pinned to an object (`on`: `comp:U1`,
`pad:U1.3`, `net:GND`, `track:3`, …), a point (`at`) or an area (`area`: two corners), with a
thread of replies and a status: `.open` (to do), `.resolved` (done), `.dismissed` (not done, the
thread says why). The web app's Comment tool (`C`) and the commands `project.comments`, `project.comment`,
`project.reply` (also the status) and `project.uncomment` write them; each change is a journal
event (`comment.added`, `comment.replied`, `comment.status`, `comment.removed`), so an agent can
wait for the person's next comment. A comment on an object that has moved shows at the object.
The file is data (literals only) and is committed with the board; the
design never reads it. Times are UTC.

```pcbkit
// file: comments-demo/.pcbkit/comments.pcbkit
pcbkit 2

comments = [
  Comment {
    id = 1
    status = .resolved
    author = "user"
    time = "2026-10-15T10:20:00Z"
    on = "comp:C4"
    at = (12.5mm, 8mm)
    text = "Put this decoupling capacitor next to U1's VDD pin"
    replies = [
      CommentReply {
        author = "agent:claude"
        time = "2026-10-15T10:22:41Z"
        text = "Moved C4 to 1.2 mm from pin 1; re-routed."
      },
    ]
  },
  Comment {
    id = 2
    author = "user"
    area = [(30mm, 5mm), (42mm, 14mm)]
    at = (42mm, 14mm)
    text = "Keep this corner free for the mounting bracket"
  },
]
```

## 2. Conventions

- **Units are part of the value:** `0.25mm`, `90deg`, `100nF`, `4.7kohm`, `60s`, `10mm2`. A
  length without a unit is an error (`at = (5, 5)` → "`5` has no unit: write `5mm`").
- **Temperatures:** a temperature on the scale is in `degC` (`85degC`: an ambient, a junction's
  limit, a laminate's Tg), a difference in `K` (`10K`: a track's rise); a thermal resistance is
  `K/W`. Two temperatures subtract into a difference, a difference moves one (`25degC + 0.5W *
  120K/W` is `85degC`); a temperature is never multiplied, and `rise = 10degC` is an error. The
  JSON of the model and the commands is SI: temperatures on the scale in °C, differences in K.
- **Coordinates:** origin at the board's lower-left corner, Y up (Gerber convention); angles
  counter-clockwise. A point is a pair: `(10mm, 5mm)`. A footprint is drawn around its origin with
  pin 1 towards −X/+Y. The standard footprints' origin is the centre of their pads (a header's
  pin rows), not pin 1: `header(9)` at `(1.27mm, 20.32mm)` puts pin 1 at `(1.27mm, 30.48mm)` and
  pin 9 at `(1.27mm, 10.16mm)` (`rotation = 90deg` turns pin 1 to −X); connectors (Footprints, below).
- **Copper layers** are values: `Cu.top`, `Cu.inner[1]` … `Cu.inner[30]`, `Cu.bottom` (KiCad's
  `F.Cu`, `In1.Cu`, `B.Cu`).
- **Nets are declared** and referred to by name: `net GND`, `net V3V3 { name = "+3V3" }` (the export name
  differs from the name in the source). A string is never a net: `a -- "GND"` is an error.
  Name nets as a schematic does: the exports carry the names, and KiCad, the fab's and
  every reviewer's tools read a net's role from its name. A rail's name says its voltage
  (`V3V3`, `V5V`, `V1V8`, `VCORE_1V1`, `VBUS`, `VBAT`, `GND`; the electrical rules warn,
  `erc-rail-name`); a signal's says its interface and function (`I2C_SDA`, `USB_DP`, `UART_TX`,
  `SPI_MOSI`, `CSI_D0_P`, `PCIE_TX0_P`, `ETH_TX_P`), with the instance where there are several
  (`CAM0_CSI_D0_P`, `I2C1_SCL`).
- **Enum values** start with a dot: `.bottom`, `.solid`, `.tracks`.
- **Names:** a component's name is its refdes (`R7 = …`); give a refdes that is not a name with
  `refdes = "U$1"`.

## 3. The design

The design is a module: declarations and `name = value` bindings in any order (a name may be
used before it is declared). What the design contains is found **by type**: every component,
net class, pour, keepout, rule area and copper graphic bound in the entry module or inside the
block instances it binds (directly, or in lists and maps) is part of the board. A design has
at most one `board = Board {…}` (none: a board named "board" with an automatic outline), and at
most one `Rules`, `Routing`, `Placer` and `Interop`.

### Board

```pcbkit
board = Board {
  name = "four-layer"
  description = "…"              // optional
  outline = rect(50mm, 40mm, radius = 2mm)   // a closed polygon: rect(w, h, radius), polygon([…]), circle(…)
  cutouts = [circle((4mm, 4mm), 1.6mm)]
  thickness = 1.6mm              // default
  layers = [signal(Cu.top), power(Cu.inner[1], label = "GND"), power(Cu.inner[2]), signal(Cu.bottom)]
  fab { finish = .enig; mask = .black }   // how it is made (below); defaults: matte black, plated edges, HASL
}
```

A board of several pieces made together (two boards side by side, a board and its separate
daughter board, a panel) gives `outline` a list of closed polygons, one per piece; `cutouts`
are holes in whichever piece holds them:

```pcbkit
board = Board {
  outline = [rect(150mm, 50mm), rectangle((0mm, 60mm), (72mm, 97mm))]
  cutouts = [circle((4mm, 4mm), 1.6mm), circle((4mm, 64mm), 1.6mm)]
}
```

Each piece is a board of its own wherever copper is concerned: § Several boards.

`outline` may be left to the engine: `outline = auto()` (the default; also `.auto`) is the
smallest rectangle around the courtyards of every component, grown by `margin` (2mm), its
corners on multiples of `grid` (1mm), rounded by `radius` (1mm: half the margin, so the arc
stays in the band the margin keeps clear, even beside a part on an edge): `auto(margin = 3mm,
radius = 2mm)`, `auto(radius = 0mm)` for square corners. A size the person names is a
requirement: `auto(max = (60mm, 40mm))` keeps the board at most 60 × 40 mm (smaller where the parts
allow; the plan says before the build when they cannot fit, `plan-size`), and `rect(60mm, 40mm)`
fixes it (an enclosure's, a HAT's: then put the parts that reach it on its edges). Its edges run through the faces of the
parts on an edge or in a corner (`edge(…)`, `corner(…)`, below; mounting holes:
`MountingHoles {}`). A board placed wholly by the engine starts at the origin. An outline the
design does not state stays the engine's: it is sized again around the parts at every build (an
imported board's outline is written into the design).

`device` says what the board is built into (ADR 0074; none: a bare board):

```pcbkit
board = Board { device = Device { form = .handheld, height = 10mm, depth = 6mm } }
```

`form` is `.handheld`, `.wearable`, `.wall` or `.desk` (default); `face` the side people look at
and press (`.top`, default); `height` the room the enclosure leaves over the face, `depth`
behind the back (`null`: no limit). For the parts whose source gives no side or rule of their
own, the face rules become placement rules: displays and buttons on the face, a display centred
on the board (`soft(area(.center))`), connectors that mate along the board (USB, jacks,
terminal blocks) on an edge facing out (`soft(edge())`), a battery behind the board on a
handheld or a wearable; mounting holes keep to the corners (`MountingHoles {}`). The check says
which part stands taller than the room on its side (`device-height`). A display's place is its
picture's (`area(…)`, `placement.where`; `board.get` gives its glass and active area). A service
button (BOOT, RESET: its net reaches a processor's pin that is none of its program's, or a strap
pin) keeps 12 mm from the person's buttons.

`assembly`: the sides the parts go on. `.auto` (default): one-sided where they fit (an automatic
outline grows instead; on a given one, small parts the top has no room for go on the back);
`.top`: every part on the top (`assembly-side` reports one the source puts on the back); `.both`:
small parts also go behind the pins they serve. A part on a differential pair keeps its side.

`layers`: the copper stack, top to bottom; left out (`.auto`) the engine chooses 2, 4 or 6 layers by what routes (§ Routing, ADR 0082), `[]` is `Cu.top`, `Cu.bottom`. `signal(l)`,
`power(l)` (a plane: pours and vias, not routed unless `routing.layers` names it), `mixed(l)`;
`label` is the name people use. Geometry: `rect(w, h)` (lower-left corner at the origin;
`rect(w, h, radius = 1.5mm)` rounds its corners, 9 points per quarter circle),
`rectangle(a, b)` (corners), `polygon([(x, y), …])`, `circle(center, radius)` (72 vertices),
`box(min, max)` (an axis-aligned box, for regions), `translated(points, d)`.

### Several boards
Boards that work together (a transmitter and its receiver, a remote and what it drives, a main
board and a daughter board on a cable) are one design, one piece of the outline per board: they
are designed, checked, shown and ordered together. Give each piece a size (no `auto()` per
piece), a gap of 5 mm or more between them, and say which parts go on which piece; a board that
has nothing to do with this one is a project of its own.

```pcbkit
// file: rc-car/.pcbkit/design.pcbkit
pcbkit 2
// A radio-controlled car: the transmitter (piece 1) and the receiver (piece 2). Each has its
// own battery, regulator and radio module; GND, VBAT and V3V3 are joined on each piece only.
net VBAT { voltage = 5V }
net V3V3, GND, CE, CSN, SCK, MOSI, MISO, STEER, THROTTLE, SERVO, MOTOR

board = Board {
  name = "rc-car"
  outline = [
    rect(50mm, 40mm, radius = 2mm),
    translated(rect(50mm, 40mm, radius = 2mm), (60mm, 0mm)),
  ]
}

// The transmitter: a battery, a 3.3 V regulator, the radio module, the controller's header
// and the joystick.
txPower = Ldo {
  vin -- VBAT
  vout -- V3V3
  gnd -- GND
  inputVoltage = 5V
}
J1 = header(2) { pins = [VBAT, GND] }
J2 = header(7) { pins = [GND, V3V3, CE, CSN, SCK, MOSI, MISO] }
J3 = header(9) { pins = [V3V3, GND, CE, CSN, SCK, MOSI, MISO, STEER, THROTTLE] }
J4 = header(4) { pins = [V3V3, STEER, THROTTLE, GND] }
transmitter = inside(board.outline[0], parts = [txPower, J1, J2, J3, J4])

// The receiver: a battery, a regulator, the radio module, the controller's header, the
// steering servo and the motor's speed controller.
rxPower = Ldo {
  vin -- VBAT
  vout -- V3V3
  gnd -- GND
  inputVoltage = 5V
}
J5 = header(2) { pins = [VBAT, GND] }
J6 = header(7) { pins = [GND, V3V3, CE, CSN, SCK, MOSI, MISO] }
J7 = header(9) { pins = [V3V3, GND, CE, CSN, SCK, MOSI, MISO, SERVO, MOTOR] }
J8 = header(3) { pins = [SERVO, VBAT, GND] }
J9 = header(3) { pins = [MOTOR, VBAT, GND] }
receiver = inside(board.outline[1], parts = [rxPower, J5, J6, J7, J8, J9])
```

Each piece is a board of its own wherever copper is concerned (copper cannot cross the gap): a
net is connected on each piece by itself, so the pieces may share net names (`GND` on each is
not joined to the other's), the router routes each piece's pins of a net apart, and the
fabrication files hold the pieces together (one panel) and each piece by itself (to order
apart). Placement keeps a part on one piece and its `edge(…)`, `corner(…)` on that piece's
edges. Say which piece a part goes on with `inside(board.outline[1])`, or for many parts and
block instances at once with a rule bound in the design (`receiver = inside(board.outline[1],
parts = [J5, rxPower])`); parts joined to them follow. A net carried from piece to piece
needs a connector on each (a cable): `net-across-pieces` warns where a piece has none.

A KiCad board joins a project as its next piece with `project.import { format: "kicad", mode: "piece" }` (in the web app: drop it onto
the open project and choose Add as another board): it goes beside the outline with a 5 mm gap
and joins the project's files — its components, its nets under a namespace (`rx/GND`), its net
classes under the namespace, its pours and art in the design, refdes the project has renumbered;
its positions and copper in `board.pcbkit` — and the design's `outline` grows by the board's
outline (`translated(polygon(…), (65mm, 0mm))`). Its parts are then moved, routed and checked
like any other.

### Fabrication

How the board is made — what a fab's order form asks for — is the board's `fab` (a
`Fabrication`); the thickness is the board's own `thickness`. Every field has a default, so a
new design that says nothing is a matte black FR-4 board with plated edges, white silkscreen and
ENIG (an imported board's stackup is written into the design):

| Field | Default | Values |
|---|---|---|
| `shop` | `.jlcpcb` | the fab that makes it: `.jlcpcb`, `.pcbway`, `.oshpark`, `.aisler`, `.nextpcb`. The vias follow its published minimums for the board's copper (a rule `fab: via ring` on the vias, in the `.kicad_dru` too; at 2 oz or more the larger of its via ring and plated hole ring: JLCPCB 0.254 mm), so the router's smallest via is one it makes; another shop: build again. Its standard stack is the one an empty `dielectric` means |
| `material` | `.fr4` | `.fr4`, `.aluminium`, `.flex` |
| `mask` | `.matteBlack` | solder mask colour: `.green`, `.matteGreen`, `.red`, `.yellow`, `.blue`, `.purple`, `.white`, `.black`, `.matteBlack`, `.grey`, `.orange`, `.pink`, `.clear` (see-through: the copper and laminate show); `.none`: no solder mask (bare copper under the finish, the silkscreen on the laminate) |
| `silk` | `.white` | silkscreen colour: `.white`, `.black`, `.yellow` |
| `finish` | `.enig` | `.haslLeadFree`, `.hasl` (leaded: neither with plated edges, which are ENIG treated; `project.fabrication` turning them on makes the finish ENIG), `.enig`, `.enepig`, `.osp`, `.immersionSilver`, `.immersionTin`, `.hardGold` |
| `outerCopper`, `innerCopper` | `35um` | copper thickness (35um is 1 oz, 70um 2 oz) |
| `copper` | `[]` | each copper layer's thickness, top to bottom, where the layers differ from `outerCopper` and `innerCopper` (heavier power planes): written into the KiCad export's stackup, used by the analysis; empty: those two |
| `viaFill` | `.none` | what fills the vias: `.none`, `.plugged`, `.epoxyFilled`, `.copperFilled` (capped). Whether the mask covers them (tenting) is the rules' `mask { viaOpenings = true }` |
| `goldFingers`, `edgePlating`, `castellated` | `false`, `true`, `false` | bevelled hard-gold edge fingers; copper plated over the board edge (its walls and a thin rim on both faces, ENIG treated: no HASL with them; copper drawn along the plated edges connects to it); plated half holes |
| `orderMark` | `.fab` | the fab's order number: `.fab` (where it likes), `.none`, `.specified` (where the silkscreen says), `.barcode` |
| `coverlay`, `stiffener`, `transparentFlex` | `.yellow`, `.none`, `false` | flex boards: `.yellow`, `.black`, `.white`; `.none`, `.polyimide`, `.fr4`, `.steel`; a see-through substrate |
| `tg` | `null` | FR-4's glass transition temperature (`130degC`, `150degC`, `170degC`, …): higher for lead-free reflow of thick or many-layer boards; `null`: the fab's standard |
| `impedance` | `false` | controlled impedance: the fab holds the traces' impedance to the stackup's |
| `dielectric` | `[]` | the dielectric between the copper layers, top to bottom (`[0.2104mm, 1.065mm, 0.2104mm]`); empty: the fab's standard stack — with `shop = .jlcpcb` JLCPCB's for the board's layers, thickness and copper (4 to 20 layers, its first stack-up template), else a two-layer board's core, 0.2104 mm of 7628 prepreg under the outer layers of a multilayer one, the rest of the thickness shared inside; written into the KiCad export's stackup |
| `permittivity` | `4.4` | the dielectric's relative permittivity; over JLCPCB's standard stack each prepreg and core has its own (JLCPCB's published 7628 4.4, 3313 4.1, 1080 3.91, 2116 4.16, core 4.6) |

The 3D view shows it. The KiCad export writes it into the board's stackup and `thickness`
(colours, copper, dielectric, finish, edge connector, castellated pads, edge plating, a polyimide
or aluminium dielectric; KiCad has no matte mask, via filling, order mark, coverlay or stiffener,
so those stay the design's alone); importing a KiCad board reads its stackup into the design. The web app's
fabrication panel and agents change it with the command `project.fabrication`, which edits the design
(only the options given; its `vias`: `tented` or `untented` sets the rules' `mask.viaOpenings`, a
filling `viaFill`).

**Fabrication files.** `pcbkit export` (the command `project.export { format: "fab" }`, the
web app's Export) writes what a fab and an assembly service need, from the board as checked: Gerber X2
files per layer (every copper layer with its pads, tracks, vias and pour fills; solder mask
openings; silkscreen; paste; the outline with its cutouts; protel extensions `.gtl`, `.g1`…,
`.gbl`, `.gts`, `.gbs`, `.gto`, `.gbo`, `.gtp`, `.gbp`, `.gko`), Excellon drill files (plated and
non-plated apart, blind and buried spans in files of their own, slots routed), a bill of
materials and a placement list (CSV), and a `README.md` with what an order form asks (size,
layers, smallest track, gap and hole). `<name>-gerbers.zip` is the archive to upload to the fab.
It refuses a board whose check is not `complete` unless forced (`force`), and then says so in
the summary.

The BOM has a line per part (value, footprint, orderable code in the column `LCSC Part #`,
quantity) with its designators. The code is, first, the component's `orderCode` (its part's
otherwise); else a part-number field of the component's own (`LCSC Part #`, `LCSC`, `LCSC Part`,
`partNumber`: imported boards carry them); else the part `parts.select` chose, recorded on its
component in `board.pcbkit`, while what it was chosen to meet is unchanged (a stale choice gives
no code). The MPN likewise: the component's own `MPN` field, else the chosen part's. Mounting holes,
test points and fiducials (by their resolved `kind`), components with nothing to solder (no
through pads, no pads with paste) and those with a field `dnp` / `exclude_from_bom` set to true
are not assembled. The placement list gives
each part's centre in mm from the board origin (the centre of its footprint's `body`, else its
origin; the Gerber files' coordinates), its side and its rotation
counter-clockwise as seen from the top (a bottom part's as its placement gives it, as KiCad's
position files do). Assembly services orient parts by their own footprints and tape, so the
rotation of some footprint families is corrected (the table `fab::assembly::ROTATIONS`: SOT-23,
SOT-223, SOT-89 and SOT-353/363 by 180°; QFN, DFN, QFP, SOIC, SOP, SSOP, TSSOP, MSOP by 270°;
polarised capacitors, micro USB and WS2812B LEDs by 180°; turned the other way on the bottom).
A component's fields `fab.rotation` (degrees, replacing its family's correction; `0` for none)
and `fab.offset` (`"x, y"` in mm, in the footprint's frame: the point placed) override it:
`fields { "fab.rotation" = "90" }`.

### Silkscreen

What is printed on the board: `Line { points; width = 0.12mm }` (a polyline, closed when it ends
where it starts), `Arc { center; radius; start; angle }` (from the angle `start` through
`angle`, counter-clockwise when positive), `Circle { center; radius; fill = false }` (radius
`0mm`: a dot as wide as its stroke),
`Polygon { points; fill = true; width = 0mm }` and `Text { text; at; size = 1mm; thickness =
0.15mm; rotation; justify = .center; vjustify = .center; mirror = null; upright = true; font = null;
italic = false; lineSpacing = 1 }` (`size`
is the height of capitals, `width` of a glyph, `null`: the size; `.left`/`.right`, `.top`/`.bottom`
say which end of the text `at` is; `\n` breaks lines, `~{…}` draws an overbar, `^{…}` and `_{…}`
super- and subscript), each on `layer = Silk.top` (default) or `Silk.bottom` (KiCad's `F.SilkS`,
`B.SilkS`). Text is drawn in KiCad's own stroke font (newstroke), laid out as KiCad lays it out,
so an imported board's text is where KiCad prints it; on the bottom it reads from below
(mirrored), on the top from above, unless `mirror` says otherwise. A footprint's text stays
readable as its part turns (KiCad's "keep upright"; `upright = false`: it turns with the part);
`font` names a TrueType face KiCad prints it in (drawn here in the stroke font at its size). Texts
print `${…}` variables as KiCad does: a part's `${REFERENCE}`, `${VALUE}`, fields,
`${NET_NAME(1)}`, `${SHORT_NET_NAME(1)}`, `${NET_CLASS(1)}`, `${FOOTPRINT_NAME}`, and the board's
`interop.kicad.textVariables` (`${TITLE}`, `${REVISION}`, …).
Drawings bound in the design (alone or in lists) are the board's. A board's `Text` without `at`
goes where the engine finds room for it once the parts are placed, clear of their courtyards,
silkscreen and pads and of the edge (lower and nearer the middle where that is equal), and is
written into `board.pcbkit` (`texts`), where it then stays: "write my name on it" is
`name = Text { text = "Alex"; size = 1.5mm }`. A label of a part names it with `near`:
`inLabel = Text { text = "IN 12-36V"; near = J1 }` goes to the free place nearest that part's
courtyard, again whenever the part moves (a connector's input, a switch's ON). In a footprint a
text without `at` is at its origin.

```pcbkit
title = Text {
  text = "four-layer rev A"
  at = (25mm, 2mm)
  size = 1.2mm
}
marks = [
  Circle { center = (3mm, 3mm), radius = 0.5mm, fill = true },
  Line { points = [(40mm, 38mm), (48mm, 38mm)], layer = Silk.bottom },
]
```

A footprint's `silk` is in its frame and flips with it (a footprint on the bottom prints on
`Silk.bottom`); in its text, `${refdes}` and `${value}` print the component's. The library's
footprints carry an IPC-7351 style silkscreen: the body's outline 0.11 mm out, cut 0.2 mm clear
of the pads (`clearOfPads(points, pads, gap, width)` does that for own footprints), a pin-1 dot
(an LED: a bracket on the cathode side), and `${refdes}` above the courtyard. Footprint text is
turned to stay readable. A component prints its refdes elsewhere with `reference = Text { text
= "${refdes}"; at = (0mm, -3mm) }` (in the footprint's frame; `text = ""` prints none), its value
with `valueText` (replacing the footprint's `${value}`). KiCad's `${REFERENCE}` and `${VALUE}`
print the same. KiCad imports keep the footprints' silkscreen and the board's, and a visible
reference or value on silk as the component's `reference` / `valueText`.

A component that does not place its reference itself gets it where a designer prints one: one
size on the board (the footprints' most common; the smallest `rules.silk` allows where a quarter
of the references find no room at it), above a part whose reference reads across the board, left
of one whose reference reads up it, else on the other sides, never on a pad, a courtyard, other
silkscreen or near the edge; where no place is free the reference is not printed.

`check` warns (KiCad's severities; `rules.severity` changes them) about silk on a pad's solder
mask opening (`silk-over-copper`, closer than `rules.silk.clearance`), text across or beyond the
board edge (`silk-edge-clearance`, `rules.silk.toEdge`; the fabrication files clip all silk to the
board less `toEdge`, so lines and outlines there are simply cut, a text would be cut unreadable), and text lower or thinner than
`rules.silk.minTextHeight` / `minTextThickness` (`text-height`, `text-thickness`; KiCad's
defaults 0.8 mm and 0.08 mm), and about the board's own text over other silkscreen on its side, a
part's outline or reference (`silk-overlap`: neither would read).

### Nets

```pcbkit
net GND
net V3V3 { name = "+3V3" }     // the export name differs from the source name
net USB_DP, USB_DN             // several at once
net CAM_D0_P, CAM_D0_N, CAM_CK_P, CAM_CK_N
net CLK { layers = [Cu.top] }  // a net's own layers (∩ its class's and the router's)
net VIN { voltage = 12V }      // a declared supply (from a connector): the electrical rules
net VMOT { current = 3A }      // the peak current it carries: the router widens its tracks on each layer (IPC-2221)
net SHIELD { role = .ground }  // what it is, where its pins do not say (.ground, .supply, .signal)
net LED[4]                     // LED[0] … LED[3], exported as LED0 … LED3
// A differential pair of two existing nets:
usb = Pair { name = "USB", p = USB_DP, n = USB_DN, impedance = 90ohm }
// Several alike:
csi = [Pair { name = commonPrefix(a.name, b.name), p = a, n = b, impedance = 100ohm } for (a, b) in [(CAM_D0_P, CAM_D0_N), (CAM_CK_P, CAM_CK_N)]]
```

What a net leaves out is resolved (ADR 0020): its role from its pins (a ground pin makes a
ground, a power pin a supply), its voltage from a power output or a block's port on it, the
current from the blocks designed for it, and last from its name (`GND`, `+3V3`, `VBUS`); a pair
from KiCad's naming (`USB_DP` and `USB_DN`). The router routes a pair the design asks to couple (a
declared `Pair`, an `impedance`, a pair rule or a class of its own; not one found by name alone) as two coupled tracks (its
class's `diffPairWidth` wide, `diffPairGap` apart, or the gap that gives its `impedance` with
tracks that wide over the board's stack, field-solved), vias side by side, uncoupled only from the pads
to the coupled section, the lengths matched by a meander on the shorter track (`docs/router.md`,
Differential pairs). `pcbkit show` gives each net's `role`, `voltage`,
`current`, `pair` and where each comes from (`why`). A block reads what a net states
(`vout.voltage`, `null` where it states nothing).

A pin may also be joined to another pin (`a -- U1.SDA`): the pins are one net, named
after the net either already has, else `Net-(R1-2)` after its first pin. Two different declared
nets joined through pins are a `short`, unless joined on purpose with `A -- B` in the design.

### Parts and components

A **part** is what is bought: footprint, typed value, pinout (logical pin → pad numbers), MPN,
fields. A **component** is a part placed on the board with its connections:

```pcbkit
part C100n = cap(100nF, package = "0603")

C1 = C100n {
  at = (20mm, 10mm)
  rotation = 90deg
  a -- V3V3
  b -- GND
}
```

Library parts: `cap(value, package = "0603", tolerance, voltage, dielectric)` (a chip size, or an
SMD electrolytic can, `"D8xL10"`: `electrolyticSizes`), `res(value,
package, tolerance, power)` (pins `a`, `b`), `led(package, color)` (pins `A`, `K`), `header(n,
rows = 1, pitch = 2.54mm)` (pins in order), `wireConnector(n, pitch = 2mm, rightAngle, smd)` (a
cable's connector: JST PH at 2.0 mm, XH at 2.5, GH at 1.25, SH at 1.0; pins in order), `hole("M3", plated = false)` (unplated: no copper of any net on either face under the screw's washer), `testPoint(diameter =
1.5mm, drill = null)`. `cap`, `res`, `led`, `inductor`, `ferrite` and `diode` also take `mpn` and
`manufacturer`: the exact part to buy (with a spec, the MPN narrows the choice to it;
`parts.search` reads the big makers' passive part numbers into these calls:
`GRM188R71H104KA93D` is `cap(100nF, package = "0603", tolerance = 10%, ratedVoltage = 50V,
dielectric = .X7R, mpn = "GRM188R71H104KA93D", manufacturer = "Murata")`). Any footprint makes a part with `Part { footprint = soic(8); pinout = {…};
pinOrder = […]; mpn = …; prefix = "U"; kind = .ic; spec = … }`; `kind` says what the part
is (`.resistor`, `.connector`, `.module`, …: the schematic's symbol, the 3D body, the rules read
it; without it the spec's category, the registry sheet the part matches, else its refdes and
footprint say); a footprint
used directly as a factory (`U1 = qfn(24) {…}`) is a part without a pinout (pins by number).
Specialise a part's data with `with`: `part Sensor: Part { footprint = soic(8) } with { pinout { GND =
"4", SDA = "5" } }`.

`Part`: `footprint`, `value` (`100nF`; printed as the BOM value), `displayValue` (text printed
instead: `"LM358"`), `pinout` (`{ VCC = "8", GND = ["4", "9"] }`: one logical pin may have several
pads), `aliases` (`{ SWDIO = "SWD" }`: another name a design may write for a pin, the name a
registry gave it before its datasheet's or a second maker's; the pad and every answer carry the
pin's own name), `pinOrder` (pad numbers for positional `pins = […]`), `mpn`, `manufacturer`, `fields`
(`{ Supplier = "…" }`), `prefix` (refdes letters of repeated components), `spec` (what the
orderable part must meet: Orderable parts, below), `orderCode` (the orderable code it is pinned
to), `marking` (what its body carries: Markings, below).

Component fields:

| Field | Default | |
|---|---|---|
| `at` | `null` | the footprint origin, `(x, y)`; `null`: the engine places it (below) |
| `rotation` | `null` | counter-clockwise; `null`: the engine turns it when it places it (among the rotations its rules allow), and a part given `at` keeps the rotation `board.pcbkit` has |
| `side` | none: the top | `.bottom` mirrors the footprint; without `side`, on a given outline where the top has no room, the engine puts a small surface-mount part (up to 8 pads, no edge or corner rule, none on a differential pair) on the bottom and records it in `board.pcbkit`, unless the board is assembled on the top alone (`Board.assembly`) |
| `mount` | the part's | how a module is held, where its footprint offers several (`moduleSocket`): `.soldered` on its own header, `.socket` in female headers, `.flush` flat on its castellated lands (`U1 = xiao.XIAO_ESP32C3 { mount = .flush }`): the part file stays as it is |
| `pins` | `{}` | connections, below |
| `place` | the part's | placement rules (below) |
| `swaps`, `pinChoices` | none | pins whose nets the router may trade (Connections, below) |
| `displayValue`, `fields` | the part's | the value printed; fields added to the part's |
| `orderCode` | the part's | pins this component to an orderable code (`parts.select { pin }` writes it) |
| `refdes` | the name | for a refdes that is not a name |
| `models` | the footprint's | 3D models of this component |
| `simulation` | the part's | physical evidence and executable behavior; an explicit value replaces the part's simulation properties |
| `reference`, `valueText` | the footprint's | where the refdes and the value are printed: `Text`s (silkscreen, below) |
| `marking` | the part's | what its body carries, over the derived marking (Markings, below) |

**Markings.** The 3D view and the films draw on each body what the real part
carries, in the notation of its kind, derived from the component (`pcbkit_core::marking`): its
value, tolerance (the spec's, a `%` in the value, a `Tolerance` field, the chosen part's), its MPN
(the part's `mpn`, an `MPN` field, the orderable part chosen for it, or a value that is a part
name: `NE555`, `1N4148`), its footprint (the package and the chip size) and its reference. Imported
boards are marked the same way from their values and fields.

| Part | Marking |
|---|---|
| through-hole resistor | colour bands per IEC 60062 from pin 1's end, the tolerance band set apart: four (two digits, multiplier, tolerance) for E6–E24 at 2 % and more, on carbon film's beige; five (three digits) for E48 and up or 1 % and less, on metal film's blue. Unknown tolerance: 5 % when two digits say the value, else 1 %. A zero ohm link: one black band |
| SMD resistor | EIA: three digits at 2 % and more (`103`, `4R7`, `R47`), four at 1 % (`1002`, `49R9`), EIA-96 on a 0603 at 1 % (`01C`, `68B`); 0402 and smaller blank; a zero ohm link `0` |
| ceramic chip capacitor, multilayer chip inductor, ferrite bead | blank |
| leaded ceramic or film capacitor | the picofarad code (`104`, `472`, `22`, `4R7`), its tolerance letter when known (`104K`); a film box also its voltage |
| aluminium electrolytic | value and voltage (`100µF 16V`) along the sleeve with the minus stripe on the negative pad's side; an SMD can: `100` / `16V` on its lid under the black half moon |
| tantalum | the picofarad code with the voltage letter (`106C`), the band on the anode (+) |
| power inductor | the microhenry code (`100`, `4R7`); an axial one: colour bands in µH on green |
| diode | the cathode band (painted grey on an SMD body, part of an axial body), the part's code (`SS14`, `T4` for a 1N4148W, `4148` on glass, `1N4007`) |
| LED | the cathode mark at the cathode's end |
| chip, regulator | the part number laser marked (`ATMEGA328P`), a suffix on the second line with a date code (`AU 1932`), a voltage on its own line (`AMS1117` / `3.3`); reading with pin 1 at its left (bottom left on a rectangle, top left on a square package) |
| transistor | the maker's code where the package is small (`1AM` for an MMBT3904, `702` for a 2N7002), else the part number (`2N3904` on a TO-92) |
| crystal | the frequency (`16.000`, `32.768K`) |
| module | the part number on the shield (`ESP32-WROOM-32`) |
| connector, switch, hole, test point | blank |

The design's `marking` (on the part or the component, the component's fields over the part's)
replaces the derived marking field by field: `marking = Marking { lines = ["LM358", "TI 2231"] }`,
`marking = "SS14"` (its lines), `Marking { bands = [.red, .red, .brown, .gold] }` (`BandColor`:
`.black` … `.white`, `.gold`, `.silver`), `Marking { coat = "#5b8fc4" }` (the body's coating),
`Marking { polarity = "2" }` (the pad on the side of the polarity mark), `Marking { lines = [];
bands = [] }` (a blank body). Printed text keeps to the face it is on: lines that do not fit at a
readable size are left out from the last.

**Placement by the engine.** A component without `at` is placed by the engine, and turned too
(among the rotations its rules allow, quarter turns by default) unless it has a `rotation`. A
component with `at` stays where it is given. The engine keeps courtyards apart, inside the board
and the areas of `inside(…)` rules, puts each part near the pins it connects to (a decoupling
capacitor next to the power pin it shares nets with), follows every placement rule (below), puts
parts with `corner(…)` (`MountingHoles {}`) in their corners and sends other parts without
connections to the corners.
`pcbkit place` (`design.run { only: "place" }`, or the placing of `pcbkit route` and `design.run`) writes its choices into `board.pcbkit`; recorded
positions are kept, so a new part does not move the others (`pcbkit place --all` places
them all anew but the locked ones), except a recorded position that no longer fits (a part given `at` on it, a
larger footprint, a smaller outline: `check` warns `placement-stale`), which `place` (and the
placement of `route`, `build`) chooses anew. Until recorded, they are computed for each evaluation
and `check` warns `placement-not-recorded` ("not placed yet") instead of judging their placement
rules. On a fixed outline `no-room` says what takes the room and where a part of that size still
fits (`side = .bottom`, outside its `inside(…)` area). Any `at` in the source wins; the command `board.pin` writes the
engine's positions into the source. Giving a placed part only `at` (`usb.J.at = (…)`,
`board.move`) moves it without turning it: it keeps the rotation the engine recorded for it until
the source gives a `rotation` (`place --all` forgets it). For a part with `edge(.any)` the edge is
the one its mating side faces, and `place-edge` says which rotation it has and which one faces
the nearest other edge. Repeated cells (a block used several times, a block's parts made per
index, a power stage's half bridges) are placed as identical copies in a row, one cell's
arrangement copied at one pitch (ADR 0086); small parts of one footprint near each other line up
on rows and columns and turn alike. `placer = Placer { spacing = 0.5mm; grid = 0.25mm }`: the
gap left between courtyards and silkscreen (room for tracks) and the grid of the positions;
where a part fits nowhere so, the engine lets its courtyard touch the others' (a crowded board;
the fabrication files then cut silk off the pads). The board's own silkscreen (`Text`s, drawings:
pin labels, a title) is kept clear the same way, on its side (drilled pads on both): parts go over
its drawings only where they fit nowhere else, never with their courtyard over one of its texts. A part with an `edge` rule on a fixed outline goes on its
edge or nowhere (`no-room` names what takes the edge), never beyond it or inside the board.
The copper follows the parts. Whatever changes a part's pads (placing, `board.move`, a drag, an
edit of the source moving, turning or flipping it, another footprint, another net on a pin,
the design file changed outside the app), the copper of other nets in their way is taken out,
the tracks and vias the change leaves joining nothing are cut back to where they still join
(a junction, a via, a pad), the pours around are refilled, all in the same undoable change; its
answer says what (`copper`: the parts, the pieces ripped, the nets now unrouted). Building
(`build: true`, `design.run`) routes only those connections again. Copper a person locked
(`locked = true` in `board.pcbkit`) is kept and listed as a conflict instead.

```pcbkit
// file: quick/.pcbkit/design.pcbkit
pcbkit 2

// Parts and connections only: no board, no positions.
net VCC, GND, SIG

J1 = header(3) { pins = [VCC, SIG, GND] }
U1 = Part {
  footprint = sot23(5)
  pinout = { IN = "1", GND = "2", OUT = "5" }
} {
  IN -- SIG
  GND -- GND
  OUT -- VCC
}
C1 = cap(100nF) {
  a -- VCC
  b -- GND
}
R1 = res(10kohm) {
  rotation = 90deg // kept: the engine only moves it
  a -- VCC
  b -- SIG
}
```

Placed, `board.pcbkit` holds where each part went; the design stays as it is.

**Placement rules.** Say what matters instead of coordinates: a component lists rules in
`place = […]`; a part gives defaults every component of it follows (a component's rule
replaces the part's of the same kind; `free()` drops them all); rules about several components
are bound in the design or a block like any other value (`ports = row([J2, J3, J4])`,
`c1AtU1 = near(C1, U1, max = 3mm)`: a rule about the first component), the one way to state
placement intent. The engine places the components without `at` so that every rule
holds and the preferences are as good as it can make them; `check` verifies every rule on every
component, placed by hand or not, and reports each broken one with its own code, the rule as
written and its line ("J1: `edge(.bottom)` (.pcbkit/design.pcbkit:12:3) does not hold: …"). Rules are
requirements; `soft(rule, weight = 2)` makes one a preference (`soft(near(x))` without `max` minimises the distance; a hard `near(x)` without `max`
keeps PCBKit's distance, as `max = ?`).
A component given `at` is fixed: the rules judge it, the engine does not move it. Each kind of
rule is a type of its own with its own fields (`edge(…)` is an `EdgeRule`, `row(…)` a `RowRule`;
all of them `PlaceRule`s), so a mistake is reported where the rule is written: `near(3mm)` is a
type error at the call (a component or a pin is expected), `row([J1, J2]) with { side = .top }`
names the field a row does not have.

| Rule | Holds when | Code |
|---|---|---|
| `edge(.bottom)` (`.top`, `.left`, `.right`, `.any`; `from`, `to`: the span along the edge from its left or bottom end; `overhang`) | the courtyard meets that edge, or sits as far inside as its pads need to keep `copperToEdge` (up to 0.5 mm further in); a footprint with a `mating` side faces out of the board, so the rotation follows; with `overhang` (a Length: `edge(.top, overhang = 6mm)`) the body reaches that far beyond the edge (and may: no `courtyard-outside-board`), the pads still `copperToEdge` inside. `place-edge` says how far to move it | `place-edge` |
| `corner(.topLeft)` (`.topRight`, `.bottomLeft`, `.bottomRight`, `.any`; `inset`) | the courtyard meets both edges of that corner (with `inset`, a Length, sits that far inside both), as far inside as the pads need to keep `copperToEdge` (up to 0.5 mm further in); in a rounded corner it moves in along the diagonal as far as the arc needs to keep the courtyard on the board and the pads `copperToEdge` from it. On a given outline: that corner of its box. `place-corner` says where to move the part | `place-corner` |
| `area(.topRight)` (`.topLeft`, `.top`, `.left`, `.center`, `.right`, `.bottomLeft`, `.bottom`, `.bottomRight`) | the courtyard's centre (a display's: its picture's) lies in that part of the board's box (a corner's or the middle ninth; an edge's name: its whole third), as a person says "the LED in the top right"; it need not touch an edge, so a mounting hole may hold the corner itself. With an automatic outline the engine places so that it holds on the outline it then sizes. `place-area` says where the part is and which way to move it | `place-area` |
| `facing(.up)` (`.down`, `.left`, `.right`, `.outward`, `.inward`) | the footprint's `mating` side points that way (`.outward`: out of its edge) | `place-facing` |
| `rotations([0deg, 180deg])`, `parallel(U1)`, `rotatedAs(J2)` | the rotation is one of these; turned like U1 or half a turn from it; exactly like J2 | `place-rotation` |
| `inside(area)`, `outside(area)` | the courtyard lies inside / clear of the area: a polygon, a `box(…)`, a keepout or rule area, `edgeBand(5mm)` (along the board's edges), `around(U1, 3mm)`; bound in the design, the components it lists (`inside(area, parts = [J5, rx])`) | `place-inside`, `place-outside` |
| `near(U1.VDD)` (PCBKit's distance: the part's role's, never closer than the two courtyards let its pads come), `near(U1.VDD, max = 2mm)`, `near(U1, max = 1mm)`, `far(U1, min = 5mm)`; in the design `near(C1, U1, max = 3mm)`, `far(U1, Q1, min = 10mm)` (about the first) | its pads on the pin's net are that close to the pin (decoupling); courtyards that close / that far apart (a part near another may touch it); a hard `near` without `max` keeps PCBKit's distance; `soft(near(U1))` minimises the distance | `place-near`, `place-far` |
| `row([J2, J3, J4], pitch = 16mm)`, `column([…], evenly = true)` (in the design) | centres on one line, in this order (left to right, bottom to top), `pitch` apart or with equal gaps | `place-row` |
| `grid([D1, D2, …, D9], columns = 3, pitch = 8mm)` (`pitchX`, `pitchY`, `evenly`; in the design) | a grid in reading order (left to right, rows from the top): a `row` per row and a `column` per column; rows and columns that share parts are placed as one piece | `place-row` |
| `together([U1, Y1, C8], max = 8mm)`, `mirror(LED1, LED2, axis = .x)`, `loop([C1.a, U1.VIN, U1.GND, C1.b], maxArea = 4mm2)` (in the design) | no two courtyards further apart than `max` (as `near`); mirror images about the board's middle; the current loop through the pins at most `maxArea` | `place-together`, `place-mirror`, `place-loop` |

A block instance takes rules for all its components: `power = Ldo { vin -- VIN, vout -- V3V3, gnd -- GND,
place = [area(.bottomLeft)] }` is as if each of its components listed `area(.bottomLeft)`;
a component's own rule of a kind (and a nested instance's) replaces the instance's of that kind.
Rules about several components (`row`, `together`, …) are bound in the design, never in a `place`.

Rules of one component that cannot both hold (`edge(.bottom)` and `facing(.up)` on a part with a
mating side, two edges (for a corner, `corner(…)`), an edge and a corner, a row of parts on a
left edge, an `area` that does not reach the part's edge or corner) are a `place-conflict` naming
both. With an automatic outline the parts on an edge
are placed around the rest, then the parts in corners at the corners of what is placed (grown by
the margin, moved out a grid step at a time where a corner is not free), and the outline runs
through their faces; with a given outline the parts in corners and on an edge are placed first,
then the rest. Footprints: `mating = .down`
is the side a plug enters, in the footprint's frame; for a connector without one (a component
whose resolved `kind` is `.connector`: its part's `kind`, else its spec, registry sheet or, last,
its refdes and footprint name; ADR 0020) the engine takes the side its body reaches furthest
beyond its pads.

```pcbkit
// file: hub/.pcbkit/design.pcbkit
pcbkit 2

// A USB hub's ports: the upstream connector on the top edge, the downstream ones on the bottom
// edge in an even row, facing out; the decoupling capacitor at its supply pin; the power LED in
// the top-right ninth of the board, whatever size the outline gets. No positions.
net VBUS, GND, UP, P1, P2, P3, LED

let socket = Footprint {
  name = "USB_A_Socket"
  pads = [
    Pad { number = "1", shape = shape.round(1.6mm), at = (-2mm, 0mm), kind = .tht, drill = 0.9mm },
    Pad { number = "2", shape = shape.round(1.6mm), at = (0mm, 0mm), kind = .tht, drill = 0.9mm },
    Pad { number = "3", shape = shape.round(1.6mm), at = (2mm, 0mm), kind = .tht, drill = 0.9mm },
  ]
  body = box((-4mm, -10mm), (4mm, 1mm))
  mating = .down // the plug enters from below
}

part UsbA: Part {
  footprint = socket
  prefix = "J"
  pinout = { V = "1", D = "2", G = "3" }
  place = [edge(.any)] // every USB-A of this design sits on an edge, facing out
}

U1 = soic(14) {
  "1" -- UP
  "2" -- P1
  "3" -- P2
  "4" -- P3
  "7" -- GND
  "14" -- VBUS
}
C1 = cap(100nF, package = "0402") {
  place = [near(U1["14"], max = 2mm)]
  a -- VBUS
  b -- GND
}
J1 = UsbA {
  place = [edge(.top)]
  V -- VBUS
  D -- UP
  G -- GND
}
J2 = UsbA {
  place = [edge(.bottom)]
  V -- VBUS
  D -- P1
  G -- GND
}
J3 = UsbA {
  place = [edge(.bottom)]
  V -- VBUS
  D -- P2
  G -- GND
}
J4 = UsbA {
  place = [edge(.bottom)]
  V -- VBUS
  D -- P3
  G -- GND
}
R1 = res(1kohm) {
  a -- VBUS
  b -- LED
}
D1 = led() {
  place = [area(.topRight)]
  A -- LED
  K -- GND
}
H1 = hole("M3") {}

ports = row([J2, J3, J4], evenly = true)
```


**Pins and connections.** In a component's body `pin -- X` connects a pin to a net, another pin,
or `.nc` (deliberately not connected). A name is a logical pin of the part's pinout (`SDA --
SDA`: the pin, then the module's net); a string is a pad number (`"25" -- GND`, `"MP#2" -- GND`).
A part with a `pinOrder` (connectors, two-terminal parts) also takes a list: `pins = [VCC, SDA,
SCL, GND]`. Pins not connected are not connected. Errors name the pin: "U1 has no pin `SDAA`
(did you mean `SDA`?)", "the footprint has no pad "3"". A pin of a component is `U1.SDA`
(logical; also `U1.pin.SDA`) or `U1["A1"]` (pad number), and connects from anywhere in the design
(`U1.SDA -- SDA`).

Pins the router may give other nets are the component's `swaps` and `pinChoices`:

```pcbkit
U3 = sram {
  swaps = [swap(slots = [["10"], ["9"], ["8"], ["7"]], needs = .none, name = "address")]
  pinChoices { "10" = pinChoice(allowed = ["10", "11", "29"]) }
}
```

`swap(slots, needs, name)`: pins whose nets the router may trade (`routing.freedoms = .declared`):
an SRAM's address lines, a latch's bits; a slot is one pin, or pins that move together. `needs`:
what else must follow a new assignment: `.none` (the memory does not care which cell holds which
bit), `.firmware` (default: GPIOs, a keyboard matrix's scan), `.romImage` (a ROM's lines: the
image is reordered) or `.schematic`. `pinChoices`: per pin, the pins its net may move to (a
GPIO's alternatives). A swap may not move a net with a pour, a net of a differential pair, one
net twice, or nets of different classes (`swap-not-allowed`); a net's pin must be among its
choices (`pin-option-default`).

**Orderable parts.** A part says what the part to buy must meet; `parts.select` picks a real,
orderable one from the assembler's catalog and records it on the components in
`board.pcbkit`. Every part of the standard library that is bought says what: `res`, `cap`, `led`,
`inductor` (its value, body and `current`), `diode` (its kind, package, a Zener's or TVS
diode's voltage, `ratedVoltage`, `ratedCurrent`), `fuse` (a PTC's hold current), `crystal` (its
frequency and load) carry a `spec`, the rest of their arguments add to it; the parts drawn for
one connector or chip carry that part's `mpn` (`usbC`: GCT USB4105-GF-A, `esdArray`: USBLC6-2SC6, `mosfet`: AO3400A / AO3401A, the blocks' regulators and
bridge), which an `mpn` argument replaces. `Part { …; spec = Spec { category = .regulator;
params = {…} } }` gives any part one; a `Part` with an `mpn` and no `spec` is bought by its
MPN; a resistor, capacitor or inductor a `Part` gives only a `value` is bought by that value
in its footprint's package. A part number is the part's, not a component's: `board.set` does
not set one (give the part its `mpn`, or pin a code with `parts.select { pin }`). What kind of
part a spec asks for is the part's `kind` (`res` is a `.resistor`), unless the spec narrows it
to a catalog category the kinds do not have.

| `Spec` field | Means |
|---|---|
| `category` | a catalog category narrower than the part's `kind`: `.regulator`, `.oscillator`; a diode's `.schottky`, `.zener`, `.tvs`, `.rectifier` (`null`: the kind) |
| `value` | a nominal (`10kohm`: that value, any tolerance), or a range the part's whole tolerance band must lie in: `10kohm ± 1%`, `tolerance(9kohm, 11kohm)` (a plain ratio after an electrical quantity is relative) |
| `tolerance` | the widest tolerance accepted (`1%`) |
| `package` | the catalog's package (`"0603"`, `"SOT-23-5"`); `null`: the footprint's |
| `ratedVoltage`, `ratedCurrent`, `ratedPower` | ratings the part must reach at least (`16V`, `0.1W`) |
| `dielectric` | a ceramic capacitor's: `.C0G`, `.X8R`, `.X7R`, `.X7S`, `.X6S`, `.X5R`, `.Y5V`; this one or a more stable one |
| `color` | an LED's: `.red`, `.green`, `.blue`, `.yellow`, `.orange`, `.amber`, `.white`, … (`null`: any colour) |
| `bidirectional` | a TVS diode's: `true` both ways, `false` one way |
| `params` | other attributes by the catalog's name: a quantity to equal, a range (`tolerance(300mA, 2A)`), text it contains |

Components whose parts need the same (the same spec and package, or MPN) are one group and get
one part. Among the parts that meet a need the first difference decides: enough stock for the
boards (1.5 times the pieces), the assembler's library (basic, then preferred extended, then
extended: an extended part costs a loading fee), a part the board already uses (an extended
part another group uses counts as preferred: its fee is paid once), the price of the pieces,
more stock, the code. A part pinned with `orderCode` (on the part, or on a component) is taken
as it is. Choices whose need did not change are kept (`pcbkit parts --all` chooses anew).

Where the assembler's catalog cannot be searched, the suppliers are asked by part number: a part
named by its `mpn`, and for a common part the part numbers that meet it by construction (a
chip resistor's or ceramic capacitor's series that spells its value, a stocked chip LED, the
shielded power inductors, Schottky, rectifier, Zener and TVS diodes, PTC fuses and SMD crystals
of the common series); their offers are judged like any other. The values such series come in
are the standard library's: `ptcHolds` (`ptcHold` rounds a current up to one), `fuseRatings`
(`fuseRating`, one-time fuses), `tvsStandoffs`
(`tvsStandoff`), `crystalLoads` (`crystalLoad`: the nearest); the blocks size their parts by
them. A plug-in module in female headers (`moduleSocket(…, mount = .socket)`: a
development board, a stepper driver) is assembled as its sockets: its BOM line is the female headers,
one per row (`orderCode` on the component pins the header's code), and the fabrication README
lists the module itself, bought separately and plugged in by hand. A ready-made module soldered on
its header (the default mount) or flush is bought by its own code (the Arduino Nano's); one whose
part names no code the fab stocks (a 0.96" SSD1306 OLED module, a MAX7219 display) is no BOM line
and no part the order misses: the README lists it with the sockets' modules, bought separately and
soldered by hand. A pin header, a female header
or a screw terminal (`header`, a `Part` on `pinHeader`, `pinSocket` or `terminalBlock`, a
registry connector named by what plugs into it, a KiCad board's
`PinHeader_1x04_P2.54mm_Vertical`) is bought by its build: the assembler's part of that category
with its rows and pins (`1x4P`), pitch and mounting (straight: through hole; angled: right angle).

```pcbkit
// file: sourced/.pcbkit/design.pcbkit
pcbkit 2

// Requirements instead of part numbers: `pcbkit parts` picks the orderable parts.
net V5V, V3V3, GND, LED

part R1k = res(1kohm, tolerance = 1%, ratedPower = 0.1W)
part Decoupling = cap(100nF, ratedVoltage = 16V, dielectric = .X7R, package = "0402")

U1 = Part {
  footprint = sot23(5)
  kind = .ic
  spec = Spec {
    category = .regulator
    params = { "Output Voltage" = 3.3V, "Output Current" = tolerance(300mA, 3A) }
  }
  pinout = { IN = "1", GND = "2", OUT = "5" }
} {
  at = (10mm, 10mm)
  IN -- V5V
  GND -- GND
  OUT -- V3V3
}
C1 = Decoupling {
  at = (6mm, 10mm)
  a -- V5V
  b -- GND
}
C2 = Decoupling {
  at = (14mm, 10mm)
  a -- V3V3
  b -- GND
}
C3 = cap(10uF ± 20%, ratedVoltage = 10V, package = "0805") {
  at = (14mm, 14mm)
  a -- V3V3
  b -- GND
}
R1 = R1k {
  at = (18mm, 10mm)
  a -- V3V3
  b -- LED
}
D1 = led(color = .green) {
  at = (22mm, 10mm)
  A -- LED
  K -- GND
}
J1 = header(3) {
  at = (2mm, 10mm)
  orderCode = "T60001"
  pins = [V5V, GND, V3V3]
}
```

The choice is recorded in each component's `fields` in `board.pcbkit` (and exported as the
footprint's properties, where KiCad, BOM tools and assembly services read it): the code (`LCSC
Part #`), the part's `MPN` and
`Manufacturer` (where the component has none of its own) and `pcbkit-need`, a hash of what the
part was chosen to meet (a changed need makes the choice stale) followed by the fields the choice
added. Stock, price and why each part won are in the answer of `pcbkit parts` (`parts.select`),
not in the repository: they are of the day. C1 of the example above, chosen from the synthetic
test catalog (`fixtures/parts/catalog.json`):

```text
C1 = { at = …, footprint = "C_0402_1005Metric", nets = { … }, fields = { "LCSC Part #" = "T20001", "MPN" = "EXC0402-100nF-X7R-16V", "Manufacturer" = "Example Passives", "pcbkit-need" = "8bcbdea5d4a0 MPN Manufacturer" } },
```

`check` judges the choices once a part is chosen: a component without a part (`order-part-missing`),
a choice made for another need (`order-part-stale`), a choice the catalogue had too few of
(`stock-short`) and a code the catalogue loaded now has too few of for one board (`order-stock`):
warnings while the board is being built, errors at the ready level once it is placed and routed
(a board whose parts cannot be bought is not ready). A part of the project's own sheet
(`.pcbkit/parts/<id>.json`) is ready only with its pin map verified: it agrees with the registry's
sheet of the part or its certified part certificate, or the part maker confirmed it pin for pin and
the table is still the one it sealed (`provenance.pins.seal`); else `order-pinout`. A pin map that
one of those contradicts (a swap of pins both name) is `pin-map`, an error. Commands: `parts.select`,
`parts.alternatives` (the ranked offers for a component, with why each ranks where it does, and the hosted library's drop-in substitutes for a part named by its number),
`parts.select { pin }` (writes `orderCode` into the source), `parts.bom` (the bill of materials, priced from
the offers loaded).

**Parts from datasheets.** A part the standard library and the registry lack is read from its
datasheet into a **part sheet** (JSON: identity, the package with the drawing's dimensions as
`[min, max]`, the datasheet's land pattern when it gives one, the pin table with types, ratings
with the pins they apply to, the support circuit, the 3D body as `solids` or a library
`model`, sources; `parts.sheet` gives the schema and the guide, `docs/parts.md`). `parts.create { dry_run: true }`
judges it on its own: the pin table against the package's pin count, the dimensions against
each other, the land pattern against the leads it is for and against the IPC-7351B footprint,
the ratings; it compiles and evaluates the part. `parts.create` writes `parts/<Name>.json` (the
sheet) and `parts/<Name>.pcbkit` (the module: `part <Id>: Part { … }` with the footprint — a
standard generator's call from the dimensions or the package's name, else the land pattern as
pads — the pinout, `pinTypes` with the ratings as limits, `fields { Status, Datasheet }`) and
adds `use ./parts/<Name> as <Name>` to the design; components are `<Name>.<Name> { … }` (the
module's part is named by its id).
`parts.use` does the same for a part of the registry. Status: `verified` (the pins confirmed by
an independent source), `checked` (every check passes), `draft` (written with errors, `draft:
true`): `check` warns `part-draft` for components of a draft part.

**Simulation parameters.** The part sheet's `simulation.parameters` is compiled into
`Part.simulation: SimulationParameters` and carried unchanged into each component. Each
`SimulationParameter` has an `id`, `quantity`, optional `min` / `typ` / `max` in the quantity's
unit (`typ = 184K/W`, `max = 150degC`, `max = 32mohm`, `typ = 8nC`), `conditions` and a
`SimulationEvidence` source (`url`, `revision`, `location`, `confidence`: `datasheet`, `measured`
or `estimate`). `parts.create { dry_run: true }` checks units, finite values, ordered bounds,
conditions and provenance. Junction temperatures are temperatures on the scale (`degC`); thermal
resistances `K/W`; electrical values in their units (`ohm`, `A`, `C`, `F`, `s`, `V` and their
prefixes). In the model's JSON and the sheet each value is a number in SI with its `unit`
(`"K/W"`, `"degC"`, `"ohm"`): temperatures on the scale in °C, differences in K. `thetaJb`, `thetaJa`, `thetaJc`, `thetaJcTop`, `thetaJcBottom`,
`thetaJl`, `psiJt` and `psiJb` are distinct quantities; characterization parameters must never
stand in for thermal resistances. `thetaJc` preserves a case metric whose face is unspecified;
it must not silently become `thetaJcTop`, `thetaJcBottom` or `thetaJb`.
Several rows for different conditions remain several rows;
a scenario must select them explicitly. Missing values are unknown. These facts do not imply
a complete executable circuit model for an IC. `analysis.inputs {"parts":true}` reports the board's coverage.

`SimulationParameters.electrical: SimulationElectrical?` is separate topology information.
The currently supported `.port` means independent external terminals with no implicit
internal current path between different pins. The standard `header`, `testPoint` and plated
`hole` declare it. A custom part can set
`simulation = SimulationParameters { electrical = .port }` only when that topology is correct;
a connector with switches or magnetics needs another executable model. A connector category
or footprint alone does not establish this. Sheet JSON uses `"electrical": "port"`; it survives
compilation and model-to-source export even when `parameters` is empty.

**Support circuits.** A chip comes with the circuit its datasheet asks for around it: the sheet's
`support` (parts between the chip's pins — decoupling at each supply pin, bulk capacitors,
pull-ups, a crystal with its load capacitors, series resistors, reset and boot buttons, other
registry parts — and connections such as a strap to ground) compiles into the module's block
`Support { chip = U1, … }`. Each part is held next to the pin it serves by a soft rule (a 100 nF
within 2 mm of its pad, one per pad with `each`); a crystal is the standard `Crystal` block,
its load capacitors computed from the crystal's CL and the stray capacitance; nets of the board
the circuit reaches beyond the chip's pins (a regulator's output, the USB connector's D+) are
its inputs, `null` until the design passes them (the parts on one are left out, and a required
one warns); choices are `Bool` options (`resetButton`, `pullUps`). `parts.use` adds the
component and its support circuit to the design (`support: false`: the module alone):

```pcbkit
// file: support/.pcbkit/parts/Ldo.pcbkit
pcbkit 2

/// A 3.3 V LDO in SOT-23-5 (as a part sheet compiles it, shortened).
part Ldo: Part {
  footprint = sot23(5)
  prefix = "U"
  pinout = { VIN = "1", GND = "2", EN = "3", NC = "4", VOUT = "5" }
  pinTypes = {
    VIN = io.powerIn(min = 2.5V, max = 6.5V)
    GND = .ground
    EN = io.input(max = 6.5V)
    NC = .nc
    VOUT = io.powerOut(3.3V)
  }
}

/// Its datasheet's circuit: 1 µF at the input and the output, EN tied to VIN.
block Support {
  chip: Any
  /// EN tied to VIN (`false`: drive EN from a logic output).
  alwaysOn: Bool = true

  CIN = cap(1uF, package = "0402", ratedVoltage = 10V) {
    place = [soft(near(chip.pin.VIN, max = 2mm), weight = 2)]
    a -- chip.pin.VIN
    b -- chip.pin.GND
  }
  COUT = cap(1uF, package = "0402", ratedVoltage = 10V) {
    place = [soft(near(chip.pin.VOUT, max = 2mm), weight = 2)]
    a -- chip.pin.VOUT
    b -- chip.pin.GND
  }
  chip.pin.EN -- (if alwaysOn then chip.pin.VIN else chip.pin.EN)
}
```

```pcbkit
// file: support/.pcbkit/design.pcbkit
pcbkit 2
use ./parts/Ldo as Ldo

net VIN { voltage = 5V }
net V3V3, GND

board = Board { name = "support" }

U1 = Ldo.Ldo {
  VIN -- VIN
  GND -- GND
  VOUT -- V3V3
}
// The capacitors join whatever U1's pins are connected to.
U1Support = Ldo.Support { chip = U1 }
J1 = header(3) { pins = [VIN, V3V3, GND] }
```

A pin is named by its refdes and pad number, `U1.3`, in reports. Several pads may share a
number (they are one pin). Imported boards: an unnumbered pad on a net is `#1`, `#2`, …; a pad
number used on several nets is `MP`, `MP#2`, … .

### Electrical rules (ERC)

What each pin does electrically lets `check` judge the connections, not only the copper. A part
types its pins with `pinTypes` (logical pin or pad number → type) and `pinType` (the other
pins); a component may change its own with `pinTypes`. The types (`std/electrical.pcbkit`):
`.passive`, `.input`, `.output`, `.bidirectional`, `.openDrain`, `.powerIn`, `.powerOut`,
`.ground`, `.nc` (not connected inside the part) and `.unspecified` (not judged: the default of
`Part { … }`; `cap`, `res`, `led`, `header`, `hole` and `testPoint` are `.passive`). With voltages:
`io.powerIn(min = 2.7V, max = 5.5V)`, `io.powerOut(3.3V, current = 300mA)`, `io.input(max = 3.6V)`
(the most it tolerates), `io.output(3.3V)` (its high level), `io.bidirectional(3.3V, max = 3.6V)`,
`io.openDrain(max = 5.5V)`. An input with a clamp (`io.input(max = 3.8V, clamp = 300uA)`, a sheet's
`limits { i(CFG1) <= 300uA }`) takes a pull-up above its `max` up to that current (model `clamp`, A). An input the part pulls up or down inside (an unused modem input, a
reset with its own pull-up) is `io.input(pull = .up)` / `io.input(pull = .down)`: it may stay
open, and a pull-up inside holds an open-drain line. A strapping pin, sampled when the part leaves reset
(an ESP32's boot-mode GPIOs), says the level it needs: `io.bidirectional(pull = .up, strap =
.high)`. A part whose sheet gives an I2C address carries it in the field `I2cAddress` (`"0x76
SDO"`: the address with its address pins low, then those pins from bit 0 up). A power input
fed through a resistor from a supplied net (a 100 Ω supply filter) is supplied too, as is one
fed through a diode from a supplied net (anode `A` on it, cathode `K` on the input's net: `VBUS
→ D1 → VLED`), at a forward drop (0.4 V) below it.

**A supply from off the board** can be said two ways, and they mean different things. The
net's `voltage` is the design's intent: this net is supplied at that voltage from off the board,
whatever brings it in (`net VIN { voltage = 12V }`; `0V` for a ground nothing else marks). A
pin's type is the part's fact: this connector pin brings a supply in (`pinTypes { "1":
io.powerOut(5V) }` on the part, or on the one component). Give the net's voltage when the board
is fed through a generic connector (a header, a terminal block), the pin type when the part
itself is a source (a USB receptacle's `VBUS`, a battery holder). Both are sources to the
electrical rules. The net's resolved voltage is its declared one, else its power outputs' (and
blocks' power ports'), else what its name says (ADR 0020); where a declared voltage and a source
on the net disagree, `check` reports `type-sources`. **A nominal is not a range** (ADR 0104): a
supply from off the board varies, so state what it stays in, `net VIN { voltage = 24V ± 10% }` (or
`tolerance(9V, 16V)`); the judgements of its highest (a TVS's stand-off, a part's maximum) read the
stated range, and without one they know only the nominal and say what they assumed.

**What a connector mates with** (ADR 0111) is its `mate`: the cable, module or board on its other
side, as the types of the pins that meet its own (`mate = mates.ftdi3v3`, or `mate = Mate { name =
"the Peltier module's leads", pins { "1" = io.powerIn(max = 15V), "2" = .ground } }`; `pitch` where
its contacts' pitch is fixed). The mate's pins are typed pins on the connector's nets: a plug wired
the other way round puts the mate's ground on a supply (`type-ground`), TX on TX is two drivers
(`type-drivers`), RX on RX a net nothing drives (`erc-floating-input`), a land at another pitch
than its mate's is `type-mate`; each at the connector's line. The model keeps it as the
component's `mate` (`name`, `pins` by pad, `pitch`). **What a capacitor decouples** is its `decouples`
(`C3 = cap(100nF) { decouples = U1.VDD }`): it is placed next to that pin (an implied preference at
the role's 2 mm); with no connections of its own its `a` joins that pin and its `b` the part's
ground pin (its pin typed `.ground`, else one named `GND` or `VSS`); wired by hand it must be
across the pin's net and a ground, else `type-decouples` at the line that wired it elsewhere; the model keeps it as `decouples` (`ref`,
`pads`).

```pcbkit
// file: erc/.pcbkit/design.pcbkit
pcbkit 2

// A sensor on a 3.3 V regulator; the I2C pull-ups go to 3.3 V, within what its pins tolerate.
net VIN { voltage = 5V }
net V3V3, GND, SDA, SCL

part Sensor: Part {
  footprint = soic(8)
  pinout = { VDD = "8", GND = "4", SDA = "5", SCL = "6", NC = "2" }
  pinTypes = {
    VDD = io.powerIn(min = 1.7V, max = 3.6V)
    GND = .ground
    SDA = io.openDrain(max = 3.6V)
    SCL = io.input(max = 3.6V)
    NC = .nc
  }
}

U1 = Sensor {
  VDD -- V3V3
  GND -- GND
  SDA -- SDA
  SCL -- SCL
}
ldo = Ldo {
  vin -- VIN
  vout -- V3V3
  gnd -- GND
}
pullups = I2cPullups {
  sda -- SDA
  scl -- SCL
  vcc -- V3V3
}
J1 = header(4) { pins = [VIN, GND, SDA, SCL] }
```

The rules, on typed pins only (an untyped design, an imported board, has nothing to report):

| Code | Finds |
|---|---|
| `type-drivers` | outputs or power outputs of two parts on one net (the type checker, ADR 0104: at the connection written last; a proof where every pin type is the design's) |
| `erc-no-source` | a net feeding power inputs with no power output, ground pin, declared voltage, or resistor or diode from a supplied net on it |
| `erc-floating-input` | an input or power input not connected, or a net of inputs only (inputs with a `pull` inside the part may stay open) |
| `type-pullup` | open-drain pins on a net without a resistor to a supply (and no push-pull output) |
| `type-sources` | sources at different voltages on one net (power outputs, ground pins, the declared voltage) |
| `type-limit` | a pin whose `max` the net's highest level exceeds (a supply, the top of the net's stated range, an output's level, a pull-up's supply: through its resistor, beyond the pin's `clamp` current; with no clamp stated, a pull-up from the part's own supply is unconfirmed); a switch's gate or base beyond its limit from its source |
| `type-operating` | a power input below its `min` (the bottom of the net's stated range where it states one) |
| `type-ground` | a ground pin on a supply, a supply pin on a ground |
| `type-nc` | a `.nc` pin wired to a net (the type checker, at the connection) |
| `type-current` | a net's stated current above what a part it flows through carries continuously, with its class's derating: a fuse (75 %), a PTC (its hold current), a diode, an inductor or ferrite, a connector's contacts (the part's rating, else its family's), a power switch's drain (at the connection to the net) |
| `type-voltage` | the voltage on a part's terminals above its rating: a capacitor's rated voltage (and its class's derating), a power switch's drain-source or collector-emitter rating with its margin (1.25 ×, twice in a bridge), a TVS whose breakdown its rail reaches, a supply pin above its recommended range; the voltage is the top of the net's stated range (`voltage = 24V ± 10%`), else its nominal |
| `type-clamp` | a TVS's clamp (its sheet's clamping voltage, else the SMxJ row for its stand-off: 9.2 V at 5 V) above the absolute maximum of a supply pin it guards (a USB TVS before a 6 V regulator) |
| `type-mate` | a connector's land whose pitch is not its `mate`'s (ADR 0111) |
| `type-decouples` | a capacitor's stated pin (`decouples = U1.VDD`, ADR 0111) not across that pin's net and a ground (the decoupled part's ground pins' nets, or a ground net): on the other side of a diode, on an input |
| `erc-single-pin` | a net that reaches one pin (a warning) |
| `type-level` | an input that reads high only above its threshold (VIH, a voltage or a share of its own supply) driven by an output of a lower level (a warning) |
| `erc-decoupling` | a supply that powers chips (`U…` parts with active pins) has fewer capacitors to ground than chips: one 100 nF next to each chip's supply pin (a warning) |
| `erc-crystal-load` | a crystal (a `crystal` footprint or part, a `Y…` part) whose pin has no capacitor to ground; oscillators (a supply pin) and three-pin resonators are left alone (a warning) |
| `erc-reset-pull` | a reset or enable input (`RESET`, `NRST`, `RST`, `RUN`, `EN`, `CHIP_EN`, …) without a `pull` inside the part, on a net with neither a resistor to a supply or ground nor a driver: a button or a capacitor alone (a warning) |
| `erc-usb-cc` | a USB Type-C receptacle (pins `CC1`, `CC2`; `VBUS` not a power input) whose CC pin has no 5.1 kΩ (4.1–6.1 kΩ) to ground and no chip that handles it (a warning) |
| `erc-usb-cc-tied` | a USB Type-C receptacle's `CC1` and `CC2` on one net |
| `erc-i2c-address` | two parts sharing two signal nets (a bus) at one I2C address: their field `I2cAddress` (from the part sheet's `i2cAddress`) with the address pins as tied (to ground, a supply, or through a resistor) |
| `erc-supply-peak` | a part whose sheet asks its supply for a peak current ("current the supply must deliver": a Wi-Fi module's 500 mA) on a rail whose source's rating does not cover it and the rail's other loads (a warning) |
| `erc-overload` | a rail whose loads (power inputs' `current`, blocks' ports) draw more (by over 2 %) than its source's rating (a power output's `current`, a regulator block's port) — the power flow of the analysis (a warning) |
| `wiring-strap` | a part's configuration pins (its sheet's strap tables, or pins with a `strap` level) wired for a mode that sets something other than what the design declares (a USB PD sink requesting 28 V on a net declared 20 V: an error), not in the part's normal mode (an ESP32 that does not boot from its flash), or wired as no row of the table (warnings) |
| `wiring-feedback` | a regulator whose sheet states the voltage it holds its feedback pin at (a reference, or a fixed variant's output) and whose divider then sets an output whose whole enclosure over the parts' tolerances misses the voltage the design states on that net by more than 10 % (only the part's own sheet; a fixed 3.3 V buck under a 100k/44.2k divider: 10.9 V; the message gives the enclosure over the tolerances and the rail's parts it overstresses), or a feedback pin on ground |
| `wiring-enable` | a regulator or load switch (a part with an output or a switch node) whose enable (`EN`, `nEN`, `EN_N`, …) is held inactive in every power state where its supply is there: below 0.3 V, or an active-low one above 0.7 × its supply (a VBUS switch whose enable only a pull-up holds) |
| `wiring-backpower` | a USB device port's VBUS (Micro-B, Mini-B, B, or a Type-C receptacle with the sink's 5.1 kΩ on both CC pins) that the board itself holds above 0.8 V with nothing plugged in (a diode from the board's 5 V into the debug port's VBUS; a host port built with a sink's Rd) (a warning) |
| `wiring-flyback` | a relay's coil (a part of category `relay`, its pins `COIL…`) switched by a transistor with no diode across it, or the diode's cathode on the switched side (it shorts the supply as the transistor turns on); a coil driven by a chip or from a connector is not judged |
| `wiring-support` | a capacitor the part's sheet's datasheet circuit (`support`) puts between two of its own non-ground pins (a switcher's bootstrap, a charge pump's flying capacitor), not behind an option, missing between those pins' nets |
| `wiring-pin-load` | a pin with more or less capacitance or resistance to ground than its sheet's `loads` allow (a hub's filter pin with 1 µF where 0.1 µF is the most, a regulator's output without its minimum capacitor) |
| `wiring-contract` | a pin against what its sheet's pin row says, compiled into fixed templates: left open where it says "do not leave open", not tied where it says "must be tied to GND", joined where it says "do not connect", grounded where it says "do not connect to GND", without the capacitor or resistor it asks for ("4.7-47 uF to VSS"), fed outside the supply range it states; a sentence no template fits is not judged |

Each is reported with the file, line and column of the component (a block's component: where the
block is used), at the component's place on the board, and follows `rules.severity`
(`severity { "erc-single-pin" = .error }`).

### Current and heat (the analysis)

The analysis (`analysis.run`, the app's Review tab; [`docs/analysis.md`](analysis.md)) reads
what the pins' types say, and three more things where they are known:

- a net's `current`: the peak current it carries (`net VMOT { current = 3A }`); without it, what
  the blocks on it are sized for and what its loads say they draw (`io.powerIn(current = 5mA)`);
- a component's `dissipation`: the heat it sheds in use (`U4 = … { dissipation = 0.8W }`; not
  the rating its part must meet, `res(ratedPower = …)`); without it, what its block computes (an
  LDO's (Vin − Vout) · I, a buck's losses, an LED and its resistor), a resistor's V² / R between
  two supplies, or a part's V · I from what it draws;
- a block's ports (`output: Port = io.powerOut(3.3V, current = 150mA, net = vout)`): what a
  regulator gives and what a stage draws, for the power flow from the inputs to the loads.

```pcbkit
// file: heat/.pcbkit/design.pcbkit
pcbkit 2

// A motor driver: 3 A through its supply, about 1.2 W in the driver, an LDO for its logic.
net VMOT {
  voltage = 12V
  current = 3A
}
net V5V, GND, IN1, IN2

part Driver: Part {
  footprint = soic(8)
  pinout = { VM = "1", GND = "4", VCC = "8", IN1 = "2", IN2 = "3", OUT1 = "6", OUT2 = "7" }
  pinTypes = {
    VM = io.powerIn(max = 18V, current = 3A)
    GND = .ground
    VCC = io.powerIn(min = 4.5V, max = 5.5V, current = 5mA)
    IN1 = io.input(max = 5.5V)
    IN2 = io.input(max = 5.5V)
  }
}

U1 = Driver {
  VM -- VMOT
  GND -- GND
  VCC -- V5V
  IN1 -- IN1
  IN2 -- IN2
  dissipation = 1.2W
}
C1 = cap(10uF, package = "1206") {
  place = [near(U1.VM, max = 2mm)]
  a -- VMOT
  b -- GND
}
ldo = Ldo {
  vin -- VMOT
  vout -- V5V
  gnd -- GND
  outputVoltage = 5V
  inputVoltage = 12V
  current = 20mA
  dropout = 1V
  regulator = ldoSot23(5V, maxInputVoltage = 18V)
}
J1 = header(4) { pins = [VMOT, GND, IN1, IN2] }
```

### Scenarios: what the person asked (the intent level)

A design states what the board must do in the person's own numbers: a `Scenario` (std
`intent.pcbkit`) carries their words (`asked`), the supplies on its nets (`Supply`: the range a
supply holds for seconds or longer, a `surge` on top, `reversed`, a source's `shortCircuit`
current, the most capacitance it allows at attach, a battery's discharge `cutoff`), the loads at
its connectors' pins or on nets (`Load`) and whether the board must `.work` in it (every part in
its recommended range and derated ratings) or only `.survive` it (a fault: absolute maxima, the
protection doing its job). `environment(kind, net)` gives a standard's scenarios on one net:

| `Environment` | Scenarios (standard, clause) |
|---|---|
| `.automotive12V` | running 9–16 V, a car battery shorting with 200 A or more (ISO 16750-2 §4.2 code C); a 24 V jump start for 60 s (§4.3.1.2); a load dump suppressed to 35 V for 40–400 ms through 0.5–4 Ω (§4.6.4 test B); the battery reversed, 14 V for 60 s (§4.7) |
| `.usbDevice` | VBUS 4.75–5.25 V, at most 10 µF at attach (USB 2.0 §7.2.1, §7.2.4.1) |
| `.industrial24V` | 19.2–30 V (IEC 61131-2 §5.2) |
| `.liionCell`, `.liionPack` (`cells = 3`) | 3.0–4.2 V a cell, the board cutting the cell off at 3.0 V unless it protects itself (`protected = true`) |

```pcbkit
// file: scenarios/.pcbkit/design.pcbkit
pcbkit 2

// A 5 V LED strip's power path, sized for what the person asked: their supply and their strip
// at full white as a scenario.
net VIN {
  voltage = 5V
  current = 9.5A
}
net GND

J1 = screwTerminal(2) { pins = [VIN, GND] }
J2 = screwTerminal(2) { pins = [VIN, GND] }
C1 = cap(1000uF, package = "D10xL10", ratedVoltage = 10V) {
  a -- VIN
  b -- GND
}

full = Scenario {
  asked = "a 5 V 10 A supply; 150 WS2812B at full white"
  supplies = [Supply { net = VIN, min = 4.75V, max = 5.25V }]
  loads = [Load { at = J2["1"], current = 9A }]
}
```

`check` runs every scenario through the ratings (the intent level, `intent-*` codes, each
finding naming its scenario and the person's words): a scenario's loads flow back to the source
through every series part and replace the nets' declared currents where they say more
(`intent-current`; a net that states less than a load is a lowered requirement, `intent-load`); a
quantity a part's sheet sets from the parts on its pins (a driver's current limit, an amplifier's
power into the load's `resistance`; `docs/parts.md` § Set quantities) against the loads on those
pins (`intent-limit`); a
supply's level reaches every net one fuse, ferrite, diode or pass stage on (`intent-voltage`: a
pin's absolute maximum, a capacitor's rated voltage, a TVS's breakdown, in `.work` the parts'
recommended ranges); a surge is clamped by the TVS on its rail (`intent-clamp`: what it guards,
`intent-power`: the energy it absorbs); a reversed supply must stop at a blocker
(`intent-reverse`); and the environment's obligations hold (`intent-inrush`: the capacitance at
attach, `intent-battery-cutoff`, `intent-fuse-breaking`). A limit no part states is unchecked,
never an error.

A finding the design keeps is waived with its reason, which the check's summary lists
(`· 1 waived (J1 intent-current: "…")`), so the person reads it:

```pcbkit
@allow(.intentCurrent, "the strip runs at most 6 A: WLED's current limit, agreed with the person")
J1 = header(2)
```

A waiver is written on the member whose findings it waives (a component, a net, a block
instance) and names the finding's code (`.typeCurrent` or `"type-current"`); one that waives
nothing is a warning (`allow-unused`).

### Mains and high voltage: creepage and clearance

A supply above the SELV limits (60 V DC, 30 V r.m.s.) makes its nets hazardous, and so are the
nets a fuse, a switch or a relay's contact joins to them; an AC supply (`ac = true`, `min` and
`max` r.m.s.) above 50 V is the mains. Their copper needs the spacing IEC 60664-1 asks at their
working voltage (`safety-*`, the layout level): creepage over the board's surface from Table F.5
(F.4 in edition 2), reinforced twice the basic and never under the clearance; the clearance
through air (mains only) from the rated impulse voltage (Table F.1) and Table F.2, reinforced one
step higher, ×1.14–3.02 above 2000 m. The scenario says where the board lives (`altitude`,
`pollution` 1–3, `overvoltage` `.I`–`.IV`); unstated: pollution degree 2, category II, material
group IIIb (a laminate whose CTI is not stated), up to 2000 m. A 230 V supply is the 250 V row:
2.5 mm basic, 5.0 mm reinforced. A slot or a bare hole at least the groove width X (1 mm at
pollution degree 2) across the path makes it longer: such a path is not judged.

- A mounting hole whose footprint names its screw (`hole("M3")`) holds metal: the thread in the
  hole and a head on one side (M3: at least 5.3 mm). Hazardous copper closer to it than
  reinforced creepage is a warning (`safety-creepage`): the screw is presumed metal and people may
  touch it; an earthed one needs basic. A nylon screw is a waiver.
- An `Insulation` the design states holds its `primary` nets' copper from its `secondary` nets'
  by its `grade` (`.reinforced` by default, `.basic`): a shorter gap is an error
  (`safety-creepage`; across a slot, the air: `safety-clearance`).

```pcbkit
// file: mains/.pcbkit/design.pcbkit
pcbkit 2

// A 230 V lamp's terminals beside a 5 V board's: the mains side kept from the SELV side.
net L, N, V5, GND

J1 = screwTerminal(2) { pins = [L, N] }
J2 = screwTerminal(2) { pins = [L, N] }
J3 = screwTerminal(2) { pins = [V5, GND] }
J4 = header(2) { pins = [V5, GND] }

mains = Scenario {
  asked = "a 230 V lamp on the board"
  supplies = [Supply { net = L, min = 207V, max = 253V, ac = true }]
}
barrier = Insulation {
  primary = [L, N]
  secondary = [V5, GND]
}
```

### Net classes

```pcbkit
powerClass = NetClass {
  name = "power" // default: the binding's name
  trackWidth = 0.5mm // 0.25mm
  clearance = 0.2mm // to copper of other nets; 0.2mm
  viaDiameter = 0.6mm // vias the router places; 0.6mm / 0.3mm
  viaDrill = 0.3mm
  layers = [] // copper layers the class may use (empty: any)
  diffPairGap = null // gap inside a differential pair (null: from its impedance, else clearance)
  diffPairWidth = null // each track of a pair (null: from its impedance, else trackWidth)
  nets = [GND, V3V3, USB, glob("VBUS*")] // nets, pairs, name patterns (`*`, `?`)
  default = false // the class of every other net (KiCad's `Default`)
}
```

Controlled impedance is the net's or the pair's, not a class's: `net ANT { impedance = 50ohm }`
(a 50 Ω RF line) and `usb = Pair { name = "USB", p = DP, n = DN, impedance = 90ohm }`. The router gives the line, on
each layer, the width that gives its impedance there and the pair the width and gap that give its
differential impedance (at the class's `diffPairGap`, else its clearance, never
wider than the class's track width so the pads stay reachable; a class's `diffPairWidth` and
`diffPairGap` win where given): on an outer layer a microstrip under the solder mask, inside a
stripline between its reference planes; beside a ground pour on its own layer a grounded coplanar
line (the pour's gap from it). A layer's references are the nearest planes above and
below it — layers typed `power`, and layers a ground or supply pour covers a quarter of the board
on — or, with none on either side, its neighbouring layers; signal layers in between are
dielectric to it. The stack is read in one place, the resolved board's (`layers = .auto`: `board.pcbkit`'s
once placed): the fab's `dielectric`, else the fab's standard stack — JLCPCB's for
the board's layers, thickness and copper (`shop = .jlcpcb`, the default; each prepreg and core with
JLCPCB's published permittivity) — else a two-layer board's core (its thickness less the copper
and masks), else 0.2104 mm of 7628 prepreg under each outer layer and the rest of the thickness
shared by the inner gaps; the fab's `permittivity` and copper. Both are solved by a
2D field solver (within 0.23 % of an external FEM, `docs/analysis.md`), and `analysis.calc { kind: "impedance" }` shows the same numbers on the open board. A pair's `riseTime`
(`Pair { name = "USB", p = DP, n = DN, impedance = 90ohm, riseTime = 4ns }`) says how fast its edges are: shorter
than about a sixth of an edge's way (100 mm for USB full speed's 4 ns, 12 mm for high speed's
500 ps) it is electrically short and `check` asks no impedance of it; `UsbC2` gives full speed's
unless `highSpeed`.
Widths are for the outer layers over the plane below; order with `fab { impedance = true }` and
the fab trims them to its stack.

The class of a net: the class that lists it, else the class (first by name) one of whose
patterns matches, else the default class (the class `default` with the values above always
exists). A net listed by two classes is an error. Which layers a net may use: `routing.layers`
∩ its class's `layers` ∩ its own `layers`. The KiCad export (`.kicad_pro`) writes the classes and
the rules: the default class is KiCad's `Default` (what no class claims, such as a pad without a
net, takes its clearance there as here), and each net is assigned its class.

### Keepouts, rule areas and pours

```pcbkit
antenna = Keepout {
  forbid = [.tracks, .vias, .pours] // also .footprints, .pads (kept for KiCad)
  outline = polygon([(40mm, 0mm), (50mm, 0mm), (50mm, 10mm), (40mm, 10mm)])
  layers = [] // empty: all
  cutouts = [] // holes it does not cover
}
```

pcbkit checks `.tracks` and `.vias` (the router keeps them out), pours leave `.pours` areas empty,
and `.footprints` keeps other components' courtyards out (`keepout`). A footprint carries its
own keepouts (`FootprintKeepout`, below: a module's antenna area), which move, turn and flip
with the component. A `RuleArea` (same
fields, no `forbid`) is a named area rule conditions refer to (`insideArea(area)`).

```pcbkit
bottomGround = pour(net = GND, layers = [Cu.bottom]) {} // over the whole board
groundPlane = pour(net = GND, layers = [Cu.inner[1], Cu.bottom]) {
  name = "GND plane" // unique; the fill is stored under it (default: the binding's name)
  outline = board.outline
  connect = .solid // .thtThermal (default: reliefs on THT pads, SMD solid), .thermal, .none
  stitch = 5mm // vias of the net across it this far apart
  fence = 2mm // vias of the net along the board's edge
  returnVias = 1mm // a via of the net within 1 mm of every via of another net
}
```

`Pour`: `name`, `net`, `layers`, `outline` (`.auto`, the default: the board's outline; on a board of several pieces, their box: the fill keeps to the pieces), `priority` (0; higher fills first), `connect`,
`clearance` (the larger of it and the net classes' applies), `minThickness` (0.25mm),
`thermalGap`, `thermalSpoke` (0.5mm), `cutouts`, `smoothing` (`Smoothing { kind = .fillet;
radius = 0.5mm }` or `.chamfer`: the outline's outer corners rounded or cut first), `hatch`
(`Hatch { width = 0.5mm; gap = 1mm }`, also `angle`, `smoothing` 0–3 with `smoothingValue`,
`minHoleArea` 0.3: a hatched fill, see below), `islands`
(`.remove`, `.keep`, `.removeSmall`: under `minIslandArea`, 10mm2). A pour is filled around the
copper the way KiCad fills a zone whenever the board is read (no step to run; the fills are
cached in `.pcbkit/cache/fills/`, never stored in git). The fill is:

- the outline (smoothed, minus cutouts and areas that forbid pours) within the board, less the
  edge clearance (non-plated slots count as edge), less foreign copper and holes grown by their
  clearances, less the fill of higher-priority pours of other nets, less the outline of
  higher-priority pours of its own net (they are in charge of their own area);
- each pad of the net joins as its own `pour` setting, a custom `zone_connection` rule, or the
  pour says: `.solid` (merged), `.none` (cleared by the clearance) or `.thermal`: cleared by
  `thermalGap` and joined by four spokes from its centre, along its axes or 45° off them for a
  round pad, as wide as `thermalSpoke` but no wider than the pad (and none at all if that is under
  `minThickness`; a pad's own `thermalSpoke` under it is raised to it), reaching `thermalGap` and half of `minThickness` beyond the pad. A spoke stays
  where its end lies in fill at least `minThickness` wide (or inside a spoke of a neighbouring
  pad that ends inside it); foreign copper cuts it like the rest of the fill. One spoke joins the
  pad; `rules.minSpokes` asks for more (`thermal-spokes`, an error as in KiCad unless `severity` says otherwise, counted as KiCad counts them:
  the pieces of the pad's outline taken halfway across the gap that lie in the fill, those in a
  piece of fill that joins nothing else only where no other is, and short of the minimum the
  tracks from the pad into the fill; each layer by itself, a pad no spoke reaches is not starved);
- fill narrower than `minThickness` goes, and so do islands: pieces whose copper (across layers)
  reaches no pad, as `islands` says, unless no piece of the pour on that layer reaches one (then
  the layer stays, less what other pours of its net, of no lower priority, fill there);
- then, as KiCad 10 refills: a pour under one that lost islands fills the space they leave, and a
  pour under a higher one of its net gives way to that one's fill rather than its outline, and
  so does a pour to an overlapping one of its net and priority before it (a tie of priorities
  goes to the earlier pour in the design, for any two pours: KiCad's goes to the greater zone
  id, and the export writes ids in that order); repeated until the fills settle (8 rounds at
  most);
- a hatched pour (KiCad's hatch pattern) is filled as KiCad 10 fills one: rings as wide as the
  spokes round its thermal reliefs, the vias of its net joined like its pads (a relief with its
  ring and spokes along the axes where pads join by reliefs, else solid), and, while the fill is
  shrunk by half its minimum width, square holes `gap` wide on a grid of `width` + `gap` in
  KiCad's coordinates (the board's KiCad origin), turned by `angle`, clipped to the fill shrunk
  by the web; holes cut to under `minHoleArea` of a whole one and holes that would enclose a
  ring, foreign copper's clearance or a solid via stay filled;
- teardrops (KiCad's `$teardrop…` zones) are left empty: KiCad rebuilds them when it refills;
  with `routing.teardrops` the router draws its own (`$teardrop_padvia$`, where a track leaves a
  pad or via of its net wider than it: from across the pad, at most 2 mm, to the track's width at
  half that past the pad's edge, at most 1 mm), each where it keeps its clearances, and they are
  filled as drawn.

Stitching is the router's: `pcbkit route` joins a net's pours on different layers with vias
where their fills overlap (a top and a bottom ground pour become one plane), and drops a via
from each pad or island its tracks cut off into the net's fill on another layer. A pour asks for
more vias of its net after routing: `returnVias` (one within that distance of every via of
another net that has none: the return path of a signal changing layers), `fence` (along the
board's edge, that far apart) and `stitch` (across the pour on a lattice of that pitch), each
only where the net's fill holds it on two layers or more and the rules allow it; a via `check`
faults is taken back. Do not place
vias as components for it: they would be parts in the BOM and the placement file.

A `Guide` is a way the router keeps nets near, the person's or the agent's intent:

```pcbkit
usbLeft = Guide {
  nets = [USB_DP, USB_DN]
  path = [(2mm, 30mm), (2mm, 5mm), (15mm, 5mm)]
  width = 2mm // the corridor along the path
}
```

Steps of those nets outside the corridor cost four times as much, so they follow it where they
can and leave it where they must (to reach a pad, around copper in the way); the post-processing
never shortcuts a guided net out of it.

Copper drawn on the board without a net (KiCad's graphics, tracks and vias of no net, and text
on copper layers: its strokes, with a `Keepout` that keeps pours out of the text's box, as
KiCad's filler does) is a `Graphic {
layer = Cu.top; points = […]; radius = 0.1mm }` (`fill = false`: a stroke along the points)
bound in the design (alone or in a list). It joins nothing; copper of any net touching it is a
`short`, closer than the clearance a `clearance` violation, and pours keep clear of it. A
`Graphic` on `Mask.top` or `Mask.bottom` is an opening in the solder mask instead (KiCad's graphics
on `F.Mask` / `B.Mask`): copper of two nets exposed in one, one of them a track, via or graphic,
is a `solder-mask-bridge`; it reaches `maskMargin` beyond its shape (`null`: the board's
`rules.mask.expansion`, as KiCad grows drawn openings; text on a mask layer opens its box). A
`Graphic` on `Margin` (KiCad's layer: a routing boundary drawn inside the outline; also lines of
`layer("Edge.Cuts")` no outline is made of) is a boundary copper keeps the edge clearance from, its
stroke counted, as KiCad's edge test takes it. Copper joins copper of its net where they overlap,
not where they merely touch (as KiCad's connectivity), except two rectangular pads square to the
axes, which join touching too.

### Rules

```pcbkit
rules = Rules {
  minClearance = 0.127mm // 0.15mm; `clearance` (the floor of every clearance)
  minTrackWidth = 0.127mm // 0.15mm; `track-width`
  copperToEdge = 0.3mm // `edge-clearance` (outline and cutouts)
  courtyardMargin = 0.25mm
  minSpokes = 2 // `thermal-spokes` (an error; 0: unchecked)
  vias { minDiameter = 0.45mm, minDrill = 0.2mm, minAnnular = 0mm, microMinDiameter = 0.2mm, microMinDrill = 0.1mm } // 0: unset; micro…: KiCad's microvias
  holes { clearance = 0.25mm, toHole = 0.25mm } // 0.25mm, 0mm (unset); `hole-clearance` (pours keep it from unplated holes too), `hole-to-hole`
  mask { expansion = 0mm, minWidth = 0mm, toCopper = 0mm, viaOpenings = false } // `solder-mask-bridge`
  silk { minTextHeight = 0.8mm, minTextThickness = 0.08mm, clearance = 0mm, toEdge = 0mm }
  severity { "courtyard-overlap" = .warning } // .error, .warning, .ignore by code
}
```

Solder mask, as KiCad's `solder_mask_bridge`: a pad opens the mask on its `mask` layers (its
copper grown by its `maskExpansion`, else `mask.expansion`; a non-plated hole: its whole pad), a
via when `viaOpenings` (grown by `expansion`), and mask graphics are drawn openings. A track, via
or graphic of another net within `toCopper` of a pad's or via's opening is a bridge; so are two
openings of pads or vias of different nets closer than `minWidth` (no web of mask fits between
them; pads of a part with `allowMaskBridges` excepted); a drawn opening bridges when it holds
copper of two nets, one of them not a pad. Copper without a net bridges any other. The router
keeps out of openings that are not its pads' own (`docs/router.md`).

**Design rules** say where limits apply with a typed selector:

```pcbkit
UsbOuter = Rule {
  select = copper(pair = USB, on = .outer)
  limits = {
    trackWidth = limit(min = 0.2mm, target = 0.2mm)
    diffPairGap = limit(min = 0.15mm, max = 0.2mm)
  }
}

EdgeBase = Rule {
  select = copper()
  limits = { edgeClearance = limit(min = 1.5mm) }
}

PlatedGround = Rule {
  select = copper(net = GND)
  replaces = [EdgeBase.limits.edgeClearance]
  reason = "GND joins the plated board edge"
  limits = { edgeClearance = limit(min = 0mm) }
}
```

Selectors: `copper(net =, pair =, netClass =, nets = […], on = .outer | .inner | Cu.top)` (no
argument: everywhere), `insideArea(area)`, `intersectsArea(area)`, `ofComponent(U1)` (or a refdes
pattern `"J*"`), `routePath(a, b, netClass = class)` (copper from a pad of `a` to a pad of `b`),
`pairsWithin(parts)` (two objects both of these components: for courtyard clearances), `onB(s)`,
joined with `and`, `or`, `not`; `kicad("…")` is a KiCad expression as it is. Limits:
`limit(min, target, max)` (lengths, angles or counts), `values = […]` for `disallow`'s items.
Constraint names: `clearance`, `trackWidth`, `viaDiameter`, `holeSize`, `holeClearance`,
`holeToHole`, `edgeClearance`, `courtyardClearance`, `diffPairGap`, `diffPairUncoupled`,
`annularWidth`, `length`, `skew`, `viaCount`, `trackAngle`, `disallow`, `zoneConnection`, … (KiCad's,
in camel case; a typo is an error with a suggestion).

Authored hard limits **intersect**: where several rules apply, the largest minimum and the
smallest maximum hold, whatever their order. A rule that `replaces` another's limit replaces only
that limit where it applies (give the `reason`); other rules still apply there. pcbkit checks
`clearance`, `trackWidth`, `viaDiameter`, `holeSize`, `annularWidth`, `courtyardClearance` and
`edgeClearance` with their conditions, and `holeClearance` / `holeToHole` everywhere; the router
keeps clear of conditional hole clearances. `diffPairGap`, `diffPairUncoupled` and `skew` are
checked as KiCad checks them: a pair's pieces on one layer that run parallel with nothing between
them are coupled, at the gap between their copper; its uncoupled length is its longer net's
tracks less the coupled length within the gap range; a net's length is its tracks without what
lies inside the pads they end in, plus its vias' heights; `skew` compares the nets of a rule with
the longest of them (each pair by itself with KiCad's `within_diff_pairs`). A pair the design
declares with `pair` keeps 0.1 mm of skew and 10 mm uncoupled (its gap −10 … +25 % counts as
coupled) where no rule says: warnings. Everything else is kept for KiCad.

A `length` or `skew` limit given as a time (`skew: limit(max = 10ps)`, or KiCad 10's time-domain
rules) compares propagation delays: a net's delay is its tracks' length on each copper layer
times that layer's delay, and its vias' height through the stack times the via's, from its net
class's tuning profile (KiCad 10's, read from the project: `tuning_profiles` and a class's
`tuning_profile`); a class without one has no delay, as in KiCad. The router matches such groups
in time too.

How `check` resolves a clearance between two pieces of copper: a pad's own `clearance` if either
has one (the larger; at least `minClearance`), else the applicable `clearance` rules, else the
largest of `minClearance`, the two classes' clearances (between the two nets of a pair, the
class's `diffPairGap` where smaller) and a pour's `clearance`. Each pair of objects (a whole track,
pad or via; a pour per layer) is reported once.

Imported KiCad custom rules (`.kicad_dru`) are kept as they are, in order (where several apply,
the later wins, as in KiCad), in `interop.kicad.rules`:

```pcbkit
interop = Interop {
  kicad = KicadData {
    origin = (100mm, -80mm) // KiCad coordinates of our origin
    settings { "min_resolved_spokes" = 2 }
    rules = [
      KicadRule {
        name = "USB"
        layer = "outer"
        condition = "A.inDiffPair('USB_D')"
        constraints { "track_width" = limit(min = 0.2mm) }
      },
    ]
  }
}
```

### Routing

```pcbkit
routing = Routing {
  engine = .pathfinder     // .straight: one straight track per connection (tests)
  angles = .octilinear     // .any: shortcuts at any angle
  layers = .auto           // every layer but power planes; .all: every layer; or a list
  timeLimit = 60s           // stated: the build's budget; unstated: sized by the board (0.6 s a connection, 60 to 180 s)
  viaCost = 2.5mm          // track the router would rather add than place a via
  bendCost = 0.03mm        // … than bend 45°
  freedoms = .always       // .always: the footprints' swap groups; .declared: also swaps and
                           // pin choices; .none: every pin keeps its net
  teardrops = false        // teardrops where tracks leave pads and vias
}
```

### Checks

`require condition "message"` (a warning under `@severity(.warning)`) checks anything at
evaluation (`distance(C1, U1) <= 6mm` between two placed components' origins, a computed value);
what the board must do in the person's numbers is a scenario (above), what it keeps against a
finding an `@allow(…)`. Where parts go is said with placement
rules (Parts and components, above), which the engine follows and `check` verifies.

## 4. Footprints

- **Library** (the standard prelude), computed per IPC-7351B: `chipResistor("0603")`,
  `chipCapacitor("0402", density = .least)`, `chipLed("0603")`, `soic(14)`,
  `soic(8, ep = (2.41mm, 3.3mm))` (an exposed pad), `soicWide(16)`, `tssop(20, ep = …)`,
  `msop(10)`, `ssop(20)`, `qsop(16)`, `sot23(5)`, `sot("SOT-223")` (`sotNames`: SOT-23 variants,
  TSOT-23, SC-70, SOT-89, SOT-523/563/723), `sod("SOD-323")` (`sodNames`: SOD-123/323/523/923/882,
  SMA, SMAF, SMB, SMC, MiniMELF), `lqfp(64)`, `tqfp(44)` (or any body: `lqfp(52, size = (10mm,
  10mm), pitch = 0.65mm)`; `ep = (6mm, 6mm)`: an exposed pad), `qfn(32, size = (5mm, 5mm), ep = (3.1mm, 3.1mm))` (`ep = null`: no
  exposed pad), `dfn(8, size = (3mm, 3mm), ep = (1.66mm, 2.38mm))` (`epVias = true` on any of
  them: thermal vias in the exposed pad, 0.3 mm holes 1 mm apart, tented, its paste in windows
  between them; `thermalVias(number, size)` and `thermalPaste(size, vias)` do the same for a pad of
  an own footprint), `dpak("TO-252")`,
  `dpak("TO-263")`, `dpak("TO-252-5")`, `dpak("TO-263-5")` (5 leads, the tab pad 6), `electrolytic(6.3mm, 7.7mm)`, `dip(16)`, `bga(10, 10, 0.8mm, (9mm, 9mm))` (`ball`, `land`:
  the drawing's ball and land; `deleted = ["A1"]`: balls left out),
  `powerInductor(5)`, `powerInductorRect(7mm, 6.6mm, 3mm)`, `crystalSmd("5032")`, `rows(…)` (any
  dual-row or quad package from datasheet dimensions, with tolerances `6mm +/- 0.2mm`,
  `tolerance(0.4mm, 1.27mm)`), `pinHeader(5, rows = 2)` (`rightAngle = true`),
  `pinSocket(6)`, `moduleSocket(name, pads, outline)` (a module on its header pins:
  `socketRow(first, count, from, step)` per row, `outline` its board; `mount` how it is held
  (ADR 0074): `.soldered` (default) on its own header soldered in, `.socket` in female headers,
  `.flush` flat, castellated: on the maker's lands where it gives `land` (across and along its row) and `landShift` (their centre out from the pins); the footprint carries itself under each (`Footprint.mounts`), so a component chooses (`mount = .flush`); raised, its courtyard is its pins' rows and the room under its board a
  keepout with `below`, its `seat` the underside of its board; `spacer =
  2.5mm` its own header's plastic under its board, `pins = true` the header's pins soldered
  through it, `radius` its corners; `module = […]` the solids its board carries, heights from the
  board's top: its chip, its USB connector, a display: `displayPanel(outline, active, thickness,
  glass = 0.7mm, tape = 0mm, frame = …, ledge = …, flex = …)` from the display's outline drawing,
  its cover glass over the active area a running device's screen lights; the pieces and whole
  modules drawn as the products are in `modules` (`std/modules.pcbkit`): `modules.qfp`, `.qfn`,
  `.so`, `.sot`, `.chip`, `.crystal`, `.button`, `.usb`, `.can`, `.radio`, `.antenna`, `.hole`,
  `.castellations`, `.silk`, `.labels`, `.under` (the board's underside), and
  `modules.arduinoNano()`, `.pico("pico2")`, `.picoPlus2()`, `.xiao("rp2040")`, `.d1mini()`,
  `.proMini()`, `.espDevkit(…)`, `.oled096(names)`, `.oled13(names)`, `.tft("st7789-2.0")`,
  `.lcd1602()`, `.max7219Digits()`),
  `mountingHole("M3", plated = true)`, `testPad(1mm)`, `testPadRect(2mm, 1mm)`,
  `testPointTht(1.7mm, 1mm)`. Chips from 0201 to 2512 (also 1008 and 2220; LEDs 0201 … 1210).
- **Through-hole power packages and connectors** (pads plus `solids`, so they look like themselves
  in 3D): `to("TO-220")` (`"TO-220F"`, `"TO-220-5"`, `"TO-247"`, `"TO-92"`, `"TO-126"`, standing),
  `wireToBoard(4, 2mm)` (shrouded, polarized wire-to-board headers at 1.0, 1.25, 1.5, 2.0, 2.5 and
  2.54 mm; `rightAngle = true`, `smd = true` with two `MP` mounting pads), `boxHeader(10)` (IDC,
  2.54 or 1.27 mm), `fpc(10, pitch = 0.5mm)` (FFC/FPC, right angle or vertical, `MP` anchors), `df40(100)` (Hirose
  DF40 board-to-board, 0.4 mm),
  `terminalBlock(2, pitch = 5mm)` (2.54, 3.5, 3.81, 5.0, 5.08, 7.5 mm), `usbMicroB()`, `usbA()`,
  `dcJack()` (5.5 x 2.1 mm). Mounting pads of connectors are numbered `MP`; a connector says
  where its plug enters (`mating`).
- **Origins**: chips, packages and `bga` at the body's centre; `pinHeader`, `pinSocket`,
  `boxHeader` (so `header(n)`) at the centre of the pin field (pin 1 at `(−(rows − 1) / 2 ·
  pitch, (n − 1) / 2 · pitch)`, the top left); `wireToBoard`, `fpc`, `terminalBlock`, `usbMicroB`,
  `usbA` and `usbC()` at the middle of their row of contacts (the mounting pads `MP` and the
  body around it); `dcJack` at its centre pin; `to(…)` at the middle lead. `board.list { kind:
  "components" }` gives each component's `bounds` on the board.
- **Catalog**: `catalog["SOIC-8_3.9x4.9mm_P1.27mm"]`; `pcbkit footprints [--query soic]
  [--png dir]` lists it.
- **Own footprints** (`Footprint {…}`): `name`, `pads`, `courtyard` (a box; default: pads + body +
  `courtyardMargin`), `courtyardShape` / `courtyardBack` (exact outlines, `[Region { points = […];
  radius = … }]`), `body`, `height`, `noCourtyard`, `package` (for 3D views only: `Package { family =
  .gullwing; body = (3.9mm, 4.9mm); height = 1.75mm; pitch = 1.27mm; span = (6mm, 4.9mm);
  leadLength = 0.835mm; leadWidth = 0.41mm }`; families `.chip`, `.gullwing`, `.nolead`, `.header`,
  `.hole`, `.other`; the library fills it in, with the IPC `fillets` goals), `models` (3D model
  files placed as KiCad places them: `Model3d { path =
  "${KICAD9_3DMODEL_DIR}/Package_SO.3dshapes/SOIC-8_3.9x4.9mm_P1.27mm.step"; offset = (0mm, 0mm,
  0.1mm); rotate = (0deg, 0deg, 90deg) }`, also `scale`, `opacity`, `hide`; KiCad imports keep
  them, the KiCad export writes them back, viewers load them), `solids` (the 3D body as simple
  solids, drawn where no library model loads instead of the body guessed from `package`: a part
  without a model file — a connector, a switch — looks like itself; below), `bridges`, `swapGroups`, `graphics`,
  `silk` (its silkscreen, above), `drawings` (every other drawing, each on its `layer`: the
  fabrication drawing `Fab.top`, the user layers `User.drawings`, `User.comments`, `User.eco1`,
  `User.custom[1]`, a cutout the part needs `EdgeCuts`, adhesive `Adhesive.top`; written into the
  files, not checked), `properties` (its fields beyond reference and value: `{ "Datasheet":
  "https://…" }`), `attributes` (`FootprintAttributes { mount = .smd; excludeFromBom = true }`,
  also `excludeFromPlacement`, `boardOnly`, `allowMissingCourtyard`), `overhang` (an edge
  connector: it may hang over the board edge), `keepouts` (areas other copper and parts keep out
  of, below).

`Pad`: `number`, `also` (the pad's other contact numbers: a USB-C receptacle's `A4` that is also
`B9`; one pad, which a pin naming any of its numbers takes whole; KiCad's file draws it once per
number and the import reads such pads back as one; `[]`), `shape` (`shape.round(d)`, `shape.rect(w, h)`, `shape.roundRect(w, h, r)`,
`shape.oval(w, h)`; size `0mm` only for a drilled pad: a plated hole without a ring), `at`, `rotation`, `kind` (`.smd` | `.tht` | `.npth`), `layer` (SMD, default
`Cu.top`), `drill` (THT/NPTH), `maskExpansion` (0.05mm; 0 = the board's `rules.mask.expansion`),
`paste` (default: SMD), `pasteMargin` (the paste opening grows by it, negative: smaller; 0mm),
`pasteRatio` (and by this share of the pad's size along each axis, `-0.1`: 10 % smaller on each
side; 0), `copper` (custom pads: extra convex pieces, `[Piece { points = […];
radius = r }]`, one point = circle, two = capsule), `offset` (where the copper is centred relative
to `at`, the hole, in the pad's frame; KiCad's drill offset), `clearance` (the pad's own clearance
to other nets and to its hole, replacing the rules but not the board minimums; `null`), `slot`
(an oval hole: its size along the pad's X and Y, `drill` the smaller; `null` = round; milled, so
`hole-to-hole` leaves it out, as KiCad does), `holeCopper` (the copper layers a non-plated hole is
on: `.all`, KiCad's `*.Cu`; `.outer`, `F&B.Cu`; `.none`, a peg with only its mask, which copper
checks pass by as KiCad's do while fills and routes keep clear of its hole), `unusedLayers` (a through pad's copper: `.keep`, `.remove`,
`.removeInner`, as a via's), `pour` (how the pad joins pours of its net where it differs from the
pour: `PadPour { connect = .solid; thermalGap = 0.3mm; thermalSpoke = 0.4mm; spokeAngle = 45deg }`,
each `null` the pour's; `spokeAngle` defaults to 45° for round pads, 0° for the others; `null`),
`mask` (the solder mask layers the pad opens where they differ from its copper: `[Mask.top,
Mask.bottom]` for an edge connector's pad on `Cu.top`, `[]` for none; `null`: an SMD pad its
side's, a hole both).
A footprint's `graphics` are copper drawn apart from its pads (`Graphic { layer = Cu.top; points
= […]; radius = r }`, one point a circle, `fill = false` a stroke); they have no net and join
nothing. Its `maskOpenings` are openings in the solder mask apart from the pads' (a logo, an
exposed strip), drawn the same way on `Mask.top` / `Mask.bottom` (the footprint's side, as a pad's
`layer`); `allowMaskBridges = true` says copper of two nets in them is intended (a solder jumper's
opening over its pads). Its `pasteOpenings` are openings in the paste stencil apart from the
pads' (an exposed pad's paste in windows), on `Paste.top` / `Paste.bottom`.

A footprint's `keepouts` are areas other copper and parts must keep out of: a module's antenna
and the clearance its layout guidelines ask for, a sensor's opening. `FootprintKeepout { outline
= rectangle((-24mm, 6.75mm), (24mm, 27.75mm)); forbid = [.tracks, .vias, .pours, .footprints] }`:
`outline` in the footprint's frame (`cutouts` holes in it), `layers` on the footprint's side as a
pad's `layer` (`[Cu.top]`: its own side; empty, the default: every copper layer), `forbid` (default
`[.tracks, .vias, .pours]`: no copper; `.footprints` keeps other components' courtyards out,
`.pads` is kept for KiCad; with `.footprints`, `below = 2.2mm` lets parts no taller than that stand
inside: the room under a raised module, ADR 0074). They move, turn and flip with the component: `check` reports tracks,
vias and other parts inside (`keepout`), the router routes around them, pour fills leave them
empty, and the KiCad export writes them as the footprint's rule areas (the import reads them
back).

Two properties say how pins relate inside the part:

- `bridges`: pads joined inside the part, as groups of pad numbers. A net that reaches one pad of
  a group reaches them all, so `check` and the router join them without copper (a push button's
  two legs per contact; KiCad's jumper pad groups). A group of one number joins the pads sharing
  it (KiCad's "duplicate pad numbers are jumpers"). Bridged pins on two nets are a `short`.
  Without a bridge, pads sharing a number are separate pads that copper must join (as in KiCad).
- `swapGroups`: pins that may trade nets without consequences off the board, so the router may
  choose (`routing.freedoms`). A group lists `slots`: a slot is one pin, or pins that move
  together (a gate's inputs and output). The library declares the two ends of resistors and
  capacitors. Parts of an imported board have none: their schematic owns their pins' nets
  (KiCad's "Update PCB from Schematic" would undo a swap); a design that allows one declares it.
  A part a requirement holds at a pin (`near(U1.pin.VCC, max = 2mm)`) trades only onto pads
  within the distance, and `check` judges the rule on the pins in effect. A trade `board.pcbkit`
  already has stays when the design later rules it out (a net since named as a pair's, a pour
  added): the board opens as it was routed, and the router moves nothing further.

```pcbkit
bridges = [["1", "2"], ["3", "4"]] // 4-pad push button
swapGroups = [SwapGroup { slots = [["1"], ["2"]] }] // resistor
```

Pads that share most attributes are copies of one with `with`; a ball grid is `ballGrid`:

```pcbkit
let ball = Pad {
  number = ""
  shape = shape.round(0.45mm)
  maskExpansion = 0mm
}
pads = ballGrid(ball, origin = (-1.2mm, 1.2mm), pitch = (0.8mm, 0.8mm), cols = 4, rows = 4)
```

## 5. Blocks and repetition

A block is a parametric piece of a board with its own names and its own frame:

```pcbkit
block LedChannel {
  input: Net
  ground: Net

  net cathode // a local net: channels[3]/cathode

  R = res(1kohm, package = "0603") {
    at = (0mm, 0mm) // in the block's frame
    a -- input
    b -- cathode
  }
  D = led(package = "0603") {
    at = (0mm, 3mm)
    A -- cathode
    K -- ground
  }
}

channels = {
  for i in 1..4 {
    [i] = LedChannel {
      input -- LED[i - 1]
      ground -- GND
      at = (6mm * i, 20mm)
    }
  }
}
```

Parameters do not capture names of the design by coincidence: shared nets are passed in.
`channels` is a map with stable keys, so the third resistor is `channels[3]/R` whatever the
source order; its refdes is given once (`R3`, from the part's `prefix`) and kept by
`board.pcbkit` (its component's `path` names the entity), so it stays when channels are added or
removed. A block instance's
`at`, `rotation`, `side` place its frame. `name: Port = value` exports an endpoint;
`instance.parts` lists its components (for rules: `pairsWithin(leftDriver.parts)`).

To change one instance without changing the block, define the setting by path:
`channels[3].R.at = (0mm, 2mm)` (coordinates stay the block's). One definition per address; one
that names nothing is an error, so is one of a setting the block itself defines.

Modules: `use ./blocks/power as power` makes the public names of another file available as
`power.Regulator`; `let` names stay private to their module. Components of a module are part of
the board only when the design binds them (or instances that contain them).

### Blocks of the standard library

The standard library has proven blocks to compose a board from instead of drawing the circuit
(the catalogue, with every parameter and equation: `docs/blocks.md` of the tool; agents:
`help.search { kind: "block" }`, `help.describe`): `Ldo`, `Buck`, `ReversePolarity`, `InputFuse` (power),
`UsbCPower`, `UsbC2`, `UsbUart` (USB), `Crystal`, `McuSupport`, `SwdHeader`, `EspSupport`
(around a microcontroller or an ESP32 module), `I2cPullups`, `LedIndicator`, `Button`,
`Divider`, `LevelShifter`, `TestPoints`, `UartHeader`, `QwiicPort` (Qwiic / STEMMA QT),
`AddressableLeds`, `MountingHoles` (a plated hole in each corner, M2 to M3 by the board's size; `screw`, `plated`,
`corners`, `net = GND` to ground them), `RaspberryPiHat` (the 40-pin header and the four M2.5
holes where the HAT specification puts them, on a board whose outline is the HAT's, `board = Board { outline = raspberryPiHatOutline() }`; GPIOs by
BCM name: `signals = { GPIO2 = SDA }`; the check holds the board to that outline), `RaspberryPiCamera` (the 15- or 22-way MIPI CSI-2 camera
connector, its lanes 100 Ω pairs), `ComputeModule` (a Compute Module 4 or 5: its two DF40
connectors and four holes where the module's are, other parts kept from under it; every other pin
by the datasheet's signal name: `signals = { PCIe_TX_P = PCIE_TX_P, CAM1_D0_P = CAM_D0_P }`, the
pairs declared), `M2KeyM` (an M.2 key M socket for an NVMe SSD or PCIe card, its standoff at
the card's `length`, the card's area kept free), `MikroBus` (a Click board socket), `PmodPort`
(Digilent Pmod host port), `ArduinoUnoShield` (the Uno R3's headers and holes, on `board = Board { outline =
arduinoUnoOutline() }`), `FeatherWing` (Adafruit Feather's strips and holes, on `board = Board { outline =
featherOutline() }`), `ArmJtag` (the 20-pin JTAG/SWD box header). Blocks that put their parts
where a standard does take `{ at = (x, y) }` to sit on the board. Standard connectors take nets by the standard's signal names; name the nets by
the interface too (`CAM_D0_P`, `PCIE_TX_P`, `I2C_SDA`). The blocks share one vocabulary: `vin`, `vout`, `vcc` and `gnd`
are nets (signals such as a crystal's `xin` may be a net or the pin itself); voltages are `…Voltage` (`outputVoltage`, `inputVoltage`, `supplyVoltage`) and default to
what the net states (`net V3V3 { voltage = 3.3V }` gives `Ldo { vin -- V5V, vout -- V3V3, gnd -- GND }` its
3.3 V), a load to the net's `current`; packages are `package`, or `<role>Package` where a block has
two kinds of part (`Button { buttonPackage = "4.2x3.2", … }`, `UsbC2 { tvsPackage = "SOD-323", … }`,
`tvsPackage = null`: none); shares are in `%` (`Buck { ripple = 20%, … }`). A block that takes a part (`Ldo { regulator = …, … }`, `Buck { converter
= …, … }`, `UsbUart { bridge = …, … }`) connects it by the usual names of its pins (`pinAliases`: `VIN` or
`IN`, `VOUT` or `OUT`, `UD+` or `UDP`, …). Each takes its nets as ports (`vin -- VBUS`) and sizes its parts from its other
parameters (an LED's resistor from the supply, forward voltage and current; a crystal's load
capacitors; I2C pull-ups from the bus capacitance and speed; a buck converter's inductor and
feedback divider), says where its parts go (soft rules: a decoupling capacitor next to its pin),
types its pins for the electrical rules, and warns when its numbers do not work (`warn`: an LDO
that sheds too much heat; `check` reports it with the line of the call). Its `port`s describe
what it gives and takes (`io.powerOut(3.3V, net = vout)`, `I2C { sda; scl }`); its documented
values are readable: `ldo.dissipation`, `status.resistance`.

```pcbkit
// file: blocks/.pcbkit/design.pcbkit
pcbkit 2

// USB-C power, a 3.3 V regulator, a status LED and an I2C bus: the blocks size and place the
// parts.
net VBUS, V3V3, GND, LED, SDA, SCL

usb = UsbCPower {
  vbus -- VBUS
  gnd -- GND
}
ldo = Ldo {
  vin -- VBUS
  vout -- V3V3
  gnd -- GND
  current = 100mA
}
status = LedIndicator {
  signal -- V3V3
  gnd -- GND
  color = .green
  current = 1mA
}
i2c = I2cPullups {
  sda -- SDA
  scl -- SCL
  vcc -- V3V3
  speed = 100kHz
}
J1 = header(4) { pins = [V3V3, GND, SDA, SCL] }
holes = MountingHoles {}
```

Parts inside a block are the library's to change only by path from the design
(`ldo.Cout.at = (2mm, 0mm)`); a block's net class is named after its path
(`buck/switchNode`).

## 6. Pin assignment

Where `routing.freedoms` allows it, `pcbkit route` may give the pins of a swap group (a
footprint's `swapGroups`, a component's `swaps`) each other's nets, and move a net to another of
its `pinChoices`: it chooses pins while it negotiates and then trades slots, tuples too, and
re-routes their nets (`freedoms = .none`: every pin keeps its net). The nets in
effect are `board.pcbkit`'s: its components' `nets` carry them, and where they differ from the design's `pins`
within the freedoms, they are the assignment; where they do not fit the freedoms (the design
changed a pin), the design's nets win. `check` verifies them; `project.export { format: "kicad" }` adds
`<name>-assign.csv` (component, pin, declared net, assigned net, what else must change: the
swap's `needs`) for firmware and schematic.

## 7. Errors

`pcbkit check` reports `status` `error` / `incomplete` / `complete`. A design that does not
evaluate reports each mistake with its code, file, line and column, for example:

| Mistake | Code | Message |
|---|---|---|
| a typo in a name | `unknown-name` | `unknown name `GDN` (did you mean `GND`?)` |
| a pin the part lacks | `unknown-pin` | `U1 has no pin `SDAA` (did you mean `SDA`?)`; `the footprint has no pad "3"` |
| a length without unit | `type` | `` `5` has no unit: write `5mm` (expected Length) `` |
| a field the type lacks | `unknown-field` | `Component has no field `rotaton` (did you mean `rotation`?)` |
| a required field missing | `missing` | `Pour needs `net`` |
| a field set twice | `duplicate` | `` `at` is set twice `` (with the first place) |
| two pours / classes with one name | `duplicate` | `two pours are named "GND"` |
| a layer not in the stack | `layer` | `pour gnd uses layer In1.Cu, which is not in the board's stack` |
| two nets joined by pins | `short` | `pins join the nets A, B: connect them explicitly …` |
| a negative size | `value` | `a test pad's diameter is positive` |
| a syntax error | `syntax` | `expected `=`, `+=`, `{` or `:` after the name, found …` |

The checks after evaluation have their own codes: `clearance`, `short`, `edge-clearance`,
`outside-board`, `keepout`, `courtyard-overlap`, `pth-inside-courtyard` and
`npth-inside-courtyard` (a pad's plated or non-plated hole touching another part's courtyard on
either side of the board, as KiCad's `pth_inside_courtyard` / `npth_inside_courtyard`: something
passes through the hole; vias are not checked; a courtyard rule with a negative minimum between
the two parts waives it), `track-width`, `via-size`, `hole-size`,
`annular-ring`, `hole-clearance`, `hole-to-hole`, `solder-mask-bridge`, `thermal-spokes`,
`layer-not-allowed`, `pour-stale` (a pour whose saved fill is not what the rules and copper make now, as KiCad's "zone fills out of date": a rule changed since it was filled; a build refills it), `diff-pair-gap` (a coupled piece of a pair outside its `diffPairGap` rule's
range), `diff-pair-uncoupled` (more of a pair uncoupled than its `diffPairUncoupled` rule allows; a
declared pair without a rule: its interface's standard, USB 2.0's D+/D− up to a sixth of their
edges' way and beyond it where its lines apart are not 90 Ω ±15 %; any pair with an impedance
coupled over less than half its length, its lines apart missing it, saying its coupled share, its
skew and how far apart its ends are: an error, a warning where it is electrically short),
`skew` (a net shorter than the longest of its `skew` rule's nets by more than the rule's maximum;
a USB 2.0 pair without a rule: the 100 ps of USB 2.0 §7.1.3),
`diff-pair-impedance` (a warning: a pair whose `impedance` its routed tracks miss by more than a
tenth, field-solved over the outer layers' coupled pieces; the report's `pairs` give each pair as
routed: lengths, skew, coupled and uncoupled length, impedance and target),
`pour-overlap` (a warning: pours of two nets over the same area of a layer at the same priority:
one fills and the other gets what is left), `track-current` (a warning: a net's tracks narrower than its stated `current`
needs on their layer for a 10 °C rise by IPC-2221: inner copper and thin copper need more than the
outer layers' 35 µm; the router routes each layer at that width and necks only at pins, and keeps
the current off layers that need over 1.5 times the narrowest one's width: inside, in thin copper;
a track carries what the parts beyond it draw, not always the net's current: a branch to a
decoupling capacitor or a divider is not held to it, and a track beside the pour or wider copper
that carries the current between its connectors, regulators and loads carries at most its share
(the net's copper solved as resistors); a neck, a narrow run short enough that its copper carries
its heat to the wider copper at its ends within 5 °C (`I²ρL²/8kA²`, its length outside its pads, at most 3.5 mm),
is not held to it),
`via-current` and `pour-current` (warnings: a via, or a pour where its current passes away from
the pads it enters at, whose share of a net's stated current rises over 20 °C by IPC-2221, the
net's copper solved with its pours as sheets and its vias as barrels),
the verifier's
layout level (warnings): `layout-antenna` (a radio's antenna keepout away from the edge, or its
open end facing more board), `layout-antenna-metal` (a mounting hole's ring or screw head in an
antenna keepout), `layout-kelvin` (a current-sense amplifier's or driver's sense line sharing copper
the measured current flows in: the share of the shunt it adds), `layout-current` (a supply or a
converter's switch node with no stated current whose copper cannot carry what its parts give it),
`layout-hardware-clearance` (a mounting screw inside an isolation a custom `clearance` rule keeps
around a class),
`track-dangling`, `via-dangling`, `swap-not-allowed`, `pin-option-default`, `silk-over-copper`, `silk-edge-clearance`, `silk-overlap`,
`text-height`, `text-thickness`, `courtyard-outside-board`, the electrical rules' `erc-…` codes
(§3), `warn` (a `@severity(.warning) require` of the design or of a block it uses), `no-room` (the engine found no place
for a component on a fixed outline, with what takes the room and where there is some),
`placement-not-recorded`, `placement-stale`, `board-oversized` (warnings; the last: a stated
outline its parts fill to under 15 %, where people's boards are 31–53 %), the parts warnings
`order-part-missing`, `order-part-stale`, `stock-short`, `order-stock`, `order-pinout` (errors once built), `part-package` (§3), the placement rules'
`place-…` codes (§3), … (`severity` changes how each is reported;
a rule's own `severity` applies to what it decides). Copper across the board edge is an
`edge-clearance` violation even at `copperToEdge = 0mm`; copper wholly outside, away from the
edge, is `outside-board` (a graphic wholly outside is not: it is cut away). Courtyards may overlap by less than 10 µm. A net is complete when its
pins and every piece of its copper are joined (a track or via of the net joined to none of its
pins is a missing connection too, as KiCad counts it); a track or via joins a fill of its net
only 5 µm deep or more (a pad on a touch). Imported boards get KiCad's severities: `hole-to-hole`
is a warning unless the project says otherwise, and what KiCad does not check (`outside-board`,
`courtyard-too-small`, `pad-overlap`) is a warning. A footprint's relations to itself are its
library's: on a board only pads of two different nets overlapping (`pad-overlap`) speak, never its
numbering or overlapping pads of one net or none; the size and layer plan (`plan-size`,
`plan-layers`) is advice before a build and is not said on a routed board.

A rule speaks at its severity only once a blind round on boards it was never tuned on has cleared
it (ADR 0097): until then its findings are **unconfirmed**, listed under their own heading
(`severity: "unconfirmed"`, the rule's own severity in `probation`; `design.check` counts them
under `unconfirmed`). They fail no level, and they are never silent: the check line counts them
("2 unconfirmed findings (plan-ground, wiring-reverse)", never "0 warnings" while there are any),
the answers name the first of them with what to do, and the verdict line ends
"· N unconfirmed findings". This covers the verifier's codes (`rating-`, `wiring-`, `intent-`,
`layout-`, `polarity-`, `mating-`, `firmware-`, `seq-`, `domain-`, `safety-`, `ready-`, `erc-`,
`plan-`, `thermal-junction`) and `track-current`, `via-current`, `pour-current`,
`pour-overlap`, `net-across-pieces`, and the geometry codes whose parity with KiCad's DRC is not
measured yet (`track-dangling`, `via-dangling`, `silk-edge-clearance`, `silk-over-copper`,
`silk-overlap`); the KiCad-parity checks, the design's
integrity and the language speak as they are. Check an unconfirmed finding against the part's
datasheet and fix it when it is real.

Where no rule says, `check` still warns about what a fab cannot make: a ring of copper around a
through hole or via narrower than 0.1 mm (KiCad's default minimum annular width; an oval pad
around a round drill: make the hole a `slot`, or the drill smaller), and a component whose
body reaches over the board edge (`courtyard-outside-board`; a footprint without a body: its
courtyard by more than IPC-7351B's nominal courtyard excess, 0.25 mm — the margin a courtyard keeps
between parts has nothing to keep apart from beyond the edge, so a header on the 0.1" grid 1.27 mm
from the edge, its body flush with it, is fine) unless it says it is meant to: `overhang = true` (a
Bool) on its footprint (connectors at the edge), or a placement rule `edge(…)` on the component
(whose `overhang` is a Length: `edge(.top, overhang = 6mm)` lets a module's antenna reach 6 mm
beyond the edge).

## 8. Examples

A minimal board:

```pcbkit
// file: minimal/.pcbkit/design.pcbkit
pcbkit 2

net VCC, GND

board = Board {
  name = "minimal"
  outline = rect(30mm, 20mm)
}

J1 = header(2) {
  at = (5mm, 10mm)
  pins = [VCC, GND]
}

C1 = cap(100nF, package = "0603") {
  at = (20mm, 10mm)
  rotation = 90deg
  a -- VCC
  b -- GND
}
```

Four layers with planes, classes, a pair, a pour, a keepout and rules:

```pcbkit
// file: four-layer/.pcbkit/design.pcbkit
pcbkit 2

net GND
net V3V3 { name = "+3V3" }
net USB_DP, USB_DN
net SENSE { layers = [Cu.top] }
USB = Pair(USB_DP, USB_DN)

board = Board {
  name = "four-layer"
  outline = rect(50mm, 40mm)
  cutouts = [circle((4mm, 4mm), 1.6mm)]
  layers = [
    signal(Cu.top),
    plane(Cu.inner[1], label = "GND"),
    plane(Cu.inner[2], label = "3V3"),
    signal(Cu.bottom),
  ]
}

powerClass = NetClass {
  name = "power"
  trackWidth = 0.5mm
  nets = [GND, V3V3, glob("VBUS*")]
}
usbClass = NetClass {
  name = "usb"
  trackWidth = 0.2mm
  clearance = 0.15mm
  diffPairGap = 0.15mm
  nets = [USB]
}

part C100n = cap(100nF, package = "0402")

U1 = qfn(24) {
  at = (25mm, 20mm)
  "1" -- USB.p
  "2" -- USB.n
  "3" -- SENSE
  "12" -- V3V3
  "25" -- GND
}

decoupling = {
  for (i, x) in enumerate([21mm, 29mm]) {
    [i] = C100n {
      at = (x, 25mm)
      rotation = 90deg
      a -- V3V3
      b -- GND
    }
  }
}

H1 = hole("M3", plated = true) {
  at = (46mm, 36mm)
  "1" -- GND
}

antenna = Keepout {
  outline = polygon([(40mm, 0mm), (50mm, 0mm), (50mm, 10mm), (40mm, 10mm)])
  forbid = [.tracks, .vias, .pours]
}

groundPlane = pour(net = GND, layers = [Cu.inner[1], Cu.bottom]) {
  name = "GND plane"
  outline = board.outline
  connect = .solid
}

rules = Rules {
  minClearance = 0.127mm
  minTrackWidth = 0.127mm
  copperToEdge = 0.3mm
  vias { minDiameter = 0.45mm, minDrill = 0.2mm }
  holes { clearance = 0.25mm, toHole = 0.25mm }
  severity { "courtyard-overlap" = .warning }
}

UsbOuter = Rule {
  select = copper(pair = USB, on = .outer)
  limits = {
    trackWidth = limit(min = 0.2mm, target = 0.2mm)
    diffPairGap = limit(min = 0.15mm, max = 0.2mm)
  }
}

EdgeGround = Rule {
  select = copper(net = GND)
  limits = { edgeClearance = limit(min = 0mm) }
}
```

Blocks in their own module, a repeated channel, pins the router may trade:

```pcbkit
// file: leds/.pcbkit/blocks/leds.pcbkit
pcbkit 2

/// One LED channel: a resistor and an LED, in the block's own frame.
block LedChannel {
  input: Net
  ground: Net

  net cathode

  R = res(1kohm, package = "0603") {
    at = (0mm, 0mm)
    rotation = 90deg
    a -- input
    b -- cathode
  }
  D = led(package = "0603", color = .red) {
    at = (0mm, 3mm)
    rotation = 90deg
    A -- cathode
    K -- ground
  }
}
```

```pcbkit
// file: leds/.pcbkit/design.pcbkit
pcbkit 2

use ./blocks/leds as leds

net GND
net LED[4]

board = Board {
  name = "leds"
  outline = rect(40mm, 25mm)
}
routing = Routing { freedoms = .declared }

U1 = soic(8) {
  at = (12mm, 10mm)
  "1" -- LED[0]
  "2" -- LED[1]
  "3" -- LED[2]
  "4" -- GND
  "7" -- LED[3]
  // GPIOs: the firmware may drive any channel from pins 1–3; LED3 may move to pin 6 or 8.
  swaps = [swap(slots = [["1"], ["2"], ["3"]], name = "channels")]
  pinChoices { "7" = pinChoice(allowed = ["7", "6", "8"]) }
}

channels = {
  for i in 1..4 {
    [i] = leds.LedChannel {
      input -- LED[i - 1]
      ground -- GND
      at = (6mm + 6mm * i, 20mm)
    }
  }
}
```

A board with its fabrication and silkscreen, an edge connector and its own reference placement:

```pcbkit
// file: fab-silk/.pcbkit/design.pcbkit
pcbkit 2

net VBUS, GND, D_P, D_N

board = Board {
  name = "fab-silk"
  outline = rect(30mm, 20mm)
  thickness = 1.2mm
  fab { mask = .matteBlack, silk = .white, finish = .enig, viaFill = .plugged }
}

let usbEdge = Footprint {
  name = "USB_Edge"
  pads = [
    Pad { number = "1", shape = shape.rect(0.6mm, 1.2mm), at = (-1.5mm, 0mm) },
    Pad { number = "2", shape = shape.rect(0.6mm, 1.2mm), at = (-0.5mm, 0mm) },
    Pad { number = "3", shape = shape.rect(0.6mm, 1.2mm), at = (0.5mm, 0mm) },
    Pad { number = "4", shape = shape.rect(0.6mm, 1.2mm), at = (1.5mm, 0mm) },
  ]
  body = box((-3mm, -1mm), (3mm, 4mm))
  overhang = true
  silk = [Line { points = [(-3mm, -1.2mm), (3mm, -1.2mm)] }, refText((0mm, -2.3mm))]
}

J1 = usbEdge {
  at = (15mm, 17mm)
  pins = { "1" = VBUS, "2" = D_N, "3" = D_P, "4" = GND }
}

C1 = cap(100nF, package = "0603") {
  at = (15mm, 9mm)
  a -- VBUS
  b -- GND
  reference = Text { text = "${refdes}", at = (-2.2mm, 0mm), size = 0.8mm, thickness = 0.12mm }
}

label = [
  Text { text = "fab-silk rev A", at = (2mm, 2mm), size = 1.2mm, justify = .left },
  Text { text = "bottom", at = (15mm, 4mm), layer = Silk.bottom },
  Circle { center = (27mm, 3mm), radius = 0.6mm, fill = true },
]
```

A footprint of the component library, as `parts.use` writes it into `.pcbkit/footprints.pcbkit`
(shortened here: KiCad's footprint also has its silkscreen, fabrication and courtyard drawings),
and a 3D model of KiCad's library by name:

```pcbkit
// file: library/.pcbkit/footprints.pcbkit
pcbkit 2

SOIC_8_3_9x4_9mm_P1_27mm = Footprint {
  name = "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm"
  description = "SOIC, 8 Pin (JEDEC MS-012AA)"
  tags = ["SOIC", "SO"]
  pads = [
    Pad { number = "1", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (-2.475mm, 1.905mm) },
    Pad { number = "2", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (-2.475mm, 0.635mm) },
    Pad { number = "3", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (-2.475mm, -0.635mm) },
    Pad { number = "4", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (-2.475mm, -1.905mm) },
    Pad { number = "5", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (2.475mm, -1.905mm) },
    Pad { number = "6", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (2.475mm, -0.635mm) },
    Pad { number = "7", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (2.475mm, 0.635mm) },
    Pad { number = "8", shape = shape.roundRect(1.95mm, 0.6mm, 0.15mm), at = (2.475mm, 1.905mm) },
  ]
  courtyard = box((-3.7mm, -2.7mm), (3.7mm, 2.7mm))
  models = [
    Model3d { path = "${KICAD10_3DMODEL_DIR}/Package_SO.3dshapes/SOIC-8_3.9x4.9mm_P1.27mm.step" },
  ]
  source = "KiCad 10.0.7 Package_SO:SOIC-8_3.9x4.9mm_P1.27mm"
  license = "CC-BY-SA-4.0 with the KiCad libraries exception"
}
```

```pcbkit
// file: library/.pcbkit/design.pcbkit
pcbkit 2

use ./footprints as fp

net VCC, GND, SDA

board = Board {
  name = "library"
  outline = rect(20mm, 15mm)
}

U1 = fp.SOIC_8_3_9x4_9mm_P1_27mm {
  at = (10mm, 7.5mm)
  "8" -- VCC
  "4" -- GND
  "5" -- SDA
}

// The library's own footprint, with KiCad's 0603 model by name.
R1 = res(4.7kohm, package = "0603") {
  at = (4mm, 7.5mm)
  rotation = 90deg
  a -- VCC
  b -- SDA
  models = [Model3d { path = "Resistor_SMD:R_0603_1608Metric" }]
}
```

A footprint's 3D body from its mechanical drawing, as `solids` (`Solid`: `shape` `.box` |
`.cylinder` (axis Z) | `.cylinderX` | `.cylinderY` | `.print` (no body: ink on the plane of
`at`, a module's silkscreen) | `.prism` (its `outline`, X across and Z up, drawn along Y over
`size.y`, less its `hole`: a bent lead's profile, a connector's mouth), `size` (X, Y, Z), `at` the centre of its
bottom face in the footprint's frame with Z up from the board, `radius` the edges' rounding,
`rotate` about X, Y, Z, `material` `.plastic` (black) | `.whitePlastic` | `.metal` | `.gold` |
`.tin` | `.copper` | `.epoxy` | `.ceramic` | `.rubber` | `.glass` | `.led` | `.screen` (a display's
cover glass over the colour it shows when off) | `.mask` (a module's solder mask, glossy, in its
`color`) | `.fr4` (a board's laminate edge) | `.solder` (a joint, freshly reflowed), `color` a hex colour instead of the material's, `hollow` a wall thickness: a tube open along Y, a connector's shell,
`top` the top face's size: a box narrowing upwards, a keycap or a knob; a cylinder's top diameter
in `top.x`, a cone; `text` lines printed on its top face, at most `textSize` high, in `ink`: a
chip's part number; a footprint whose solids print text carries no marking derived from its part).
`solid.box`, `solid.cylinder`, `solid.shell`, `solid.print(["D13"], (w, h), at = …)`,
`solid.prism([(0mm, 0mm), (1mm, 0mm), (0mm, 1mm)], 2mm)` and `frustum` build them. The
standard USB-C receptacle and the tactile switches carry theirs (`usbC()`, `button()`), and
`body` (`std/bodies.pcbkit`) has the bodies of whole parts, each in its own frame:
`body.keyswitch(kind = .mx, units = 1.25)` (a switch with its keycap; `.choc`, `.alps`, `.ulp`), `body.stabilizer(span)`,
`body.diode(pitch, package = "DO-35")`, `body.resistor(pitch)`, `body.axial(…)`,
`body.module(size, kind = "promicro", lift = 2.5mm)`, `body.jack(size, height, kind = "barrel")`
(`audio`, `audioVertical`, `rj45`, `usbA`, `usbB`, `sd`, `din`, `bnc`, `sma`, `rca`, `banana`), `body.coax(size)`,
`body.rotary(size, height, kind = "encoder")` (`pot`, `trimmer`), `body.tactile(size, height)`,
`body.part(size, height, kind = "relay")` (`buzzer`, `coin`, `cell`, `slide`, `slider`, `slot`, `lever`,
`round`, `display`, `sensor`), `body.led(size, height)`, `body.ledTht(diameter)`,
`body.connector(size, height, pins = […])` (a housing with its pins), `body.header(size, pins)` (any
number of rows), `body.capacitor(pitch, size, kind = "disc")`, `body.can(diameter, height)`,
`body.socket(size, height)`, `body.dsub(shell = "DE")` (a D-sub at the board's edge or `vertical`),
`body.toroid(diameter, thickness)`, `body.pushbutton(diameter)`, `body.standing(pitch, length, diameter, color)`
(an axial part on end), `body.onLegs(size, height, pins)`, `body.chip(length, width, height, span)`,
`body.standoff(diameter, height)` and `body.module(size, kind = "can")` (a shielded module). An imported footprint carries no solids: where no library model loads,
the renderer draws the standard body of the part it recognisably is (`pcbkit_core::bodies`: a key
switch by its centre hole, a USB receptacle by its contacts, a jack by where its courtyard reaches
beyond its pins; `pcbkit models` names it). The KiCad export does not write solids (KiCad has no
such body).

```pcbkit
// file: solids/.pcbkit/design.pcbkit
pcbkit 2

// A two-pin 5 mm terminal block: a green housing, two screw heads, the wire entries.
block5mm = Footprint {
  name = "TerminalBlock_1x02_P5.00mm"
  pads = [
    Pad {
      number = "1"
      kind = .tht
      at = (-2.5mm, 0mm)
      shape = shape.rect(2.4mm, 2.4mm)
      drill = 1.3mm
    },
    Pad { number = "2", kind = .tht, at = (2.5mm, 0mm), shape = shape.round(2.4mm), drill = 1.3mm },
  ]
  body = box((-5mm, -4.2mm), (5mm, 4.2mm))
  height = 10mm
  solids = [
    solid.box((10mm, 8.4mm, 10mm), radius = 0.3mm, color = "#1f7a3a"),
    solid.cylinder(3.2mm, 0.4mm, at = (-2.5mm, 1.2mm, 10mm), material = .metal),
    solid.cylinder(3.2mm, 0.4mm, at = (2.5mm, 1.2mm, 10mm), material = .metal),
    solid.box((3mm, 0.1mm, 2.8mm), at = (-2.5mm, -4.22mm, 2.3mm), color = "#0b0b0c"),
    solid.shell((3.6mm, 0.6mm, 3.4mm), wall = 0.3mm, at = (-2.5mm, -4.2mm, 2mm)),
    solid.box((3mm, 0.1mm, 2.8mm), at = (2.5mm, -4.22mm, 2.3mm), color = "#0b0b0c"),
    solid.shell((3.6mm, 0.6mm, 3.4mm), wall = 0.3mm, at = (2.5mm, -4.2mm, 2mm)),
  ]
}

net A, B

board = Board {
  name = "solids"
  outline = rect(20mm, 15mm)
}

J1 = block5mm {
  at = (10mm, 7.5mm)
  "1" -- A
  "2" -- B
}

R1 = res(1kohm, package = "0603") {
  at = (10mm, 2mm)
  a -- A
  b -- B
}
```

Markings derived from the parts (`103`, `01C`) and given by the design:

```pcbkit
// file: markings/.pcbkit/design.pcbkit
pcbkit 2

net VCC, GND, OUT

board = Board {
  name = "markings"
  outline = rect(30mm, 20mm)
}

R1 = res(10kohm, package = "0805") {
  at = (6mm, 14mm)
  a -- VCC
  b -- OUT
}

R2 = res(10kohm, package = "0603", tolerance = 1%) {
  at = (6mm, 6mm)
  a -- OUT
  b -- GND
}

U1 = Part {
  footprint = soic(8)
  mpn = "LM358DR"
} {
  at = (18mm, 10mm)
  marking = Marking { lines = ["LM358", "TI 2231"] }
  "4" -- GND
  "8" -- VCC
  "3" -- OUT
}

C1 = cap(100nF, package = "0603") {
  at = (26mm, 10mm)
  rotation = 90deg
  a -- VCC
  b -- GND
}
```

## 9. Recipes

**A transmitter and its receiver** (boards that work together): one design, one piece of the
outline per board (§ Several boards).

**Add a component:** declare its nets (if new) and add it with its connections; leave `at` out
to let the engine place it (`pcbkit place`), or give it.

```pcbkit
net I2C_SDA
R7 = res(4.7kohm, package = "0402") {
  at = (31.5mm, 12mm)
  a -- V3V3
  b -- I2C_SDA
}
```

**Compose from blocks:** look for a block before drawing a circuit (`help.search { kind: "block" }`; in the
tool's repository `docs/blocks.md`), declare the nets it joins, and call it with them; give the
parameters that matter (`voltage`, `current`) and read what it computed (`help.describe`).

```pcbkit
net V12 { voltage = 12V }
net V5V, GND
buck = Buck {
  vin -- V12
  vout -- V5V
  gnd -- GND
  outputVoltage = 5V
  inputVoltage = 12V
  current = 2A
}
```

**Reconnect a pin:** change one entry of the component: `SDA: SCL` instead of `SDA: SDA`. Remove
the entry to leave the pin unconnected; write `.nc` to say it stays unconnected on purpose.

**Add a ground pour:**

```pcbkit
bottomGround = pour(net = GND, layers = [Cu.bottom]) {}   // the whole board; or `{ outline = … }`
```

it is filled the next time the board is read (no step to run); `pcbkit route` routes around it.

**Give nets a class:** `powerClass = NetClass { name = "power"; trackWidth = 0.5mm; nets = [V5V, GND] }`
(a design's names hide the library's: do not call a class `power` if the board uses `power(…)`).

**Keep a net or a class off a layer:** `net CLK { layers = [Cu.top] }` or the class's `layers`;
keep everything off a layer: `routing = Routing { layers = [Cu.top, Cu.bottom] }`; a local keepout:
a `Keepout { forbid = [.tracks, .vias]; layers = […]; outline = … }`.

**Keep where the engine put a component:** `pcbkit call board.pin '{"ids": ["U1"]}'` (no
`ids`: all of them) writes the positions into the source.

**Move a component:** edit its `at` and `rotation` (and `side`). Its placement rules still hold
or `check` reports the `place-…` rule it breaks; change the rule if the move is intended.

**Place relative to another component:** `at = U1.at + (0mm, 5mm)` (follows U1 when U1 moves in
the source).

After any edit: `pcbkit check`, then `pcbkit route` if nets are unrouted or copper went
stale.

## Executable component behavior

A part runs in the device laboratory when it carries a behaviour; authoring, the models, their
parameters and fidelity are in [device environments](device-environments.md) (`lab.catalog`
lists the models). The types, in `std/parts.pcbkit` and `std/behavior.pcbkit`:

- `Part.simulation.behavior: Behavior?` (a component's `simulation: SimulationParameters?`
  overrides its part's): `model`, `ports` (physical pad numbers in protocol order: power
  `[positive, ground]`, I²C `[SDA, SCL]`, SPI `[MOSI, SCK]`, one pad for a single signal),
  `parameters`, `height` (above the mounting plane), a processor's `firmware` (the project folder
  of its program), the binary files it is linked with (`data: Map[String, FirmwareData]`,
  `FirmwareData { url; sha256; bytes }`: fetched and checked when the program is built), or in
  place of a folder the image its makers published (`image: FirmwareData?`: their release's flash
  image from offset 0, fetched and checked when the device opens, never stored in the project) and a
  button's `key` (the `KeyboardEvent.code` that operates it). A microSD socket holds a card with
  the behavior `sd-card` (`ports = { clk = [pad], cmd = [pad], dat0 = [pad] }`, the socket's pads):
  its `image` is what the card holds, its files as a zip archive (they become a FAT32 volume) or a
  raw image of the card, and the card runs in the slot of the processor those pads reach (an
  ESP32-P4's slot 0: GPIO43, GPIO44, GPIO39). A board of logic chips (ADR 0043) has a cassette in
  its `tape-player` (`image`, a `.tap` file) and may name the board as that cassette leaves it once
  loaded (`state: FirmwareData?`, saved by `lab.checkpoint` on this very board and ROM): the app
  resumes it with `lab.resume` instead of playing the tape.
- Orderable parts run as they are: registry parts with a behavior (`lab.catalog`'s `parts` per
  model: `ARDUINO_NANO`, `RP2040`, `RP2350A`, `ESP32_WROOM_32E`, `ESP32_S3_WROOM_1`,
  `STM32G431CBU6`, `STM32F103C8T6`, `SN65HVD230`, `SSD1306_OLED_I2C`, `ST7789_TFT_2_0`, `SHT31_DIS`,
  `BME280`, …), `button(size, key = "ArrowLeft")` (a pin to ground) and
  `led(…, indicator = true)` (driven from a pin through its resistor). A board to order is made of
  those.
- `behavioralPart(…)` builds such a part; the starter parts (`labMcu`, `labComputer`, `labScreen`,
  `labButton`, `labSwitch`, `labKnob`, `labIndicator`, `labBattery`, `labServo`, sensors, …) are
  envelopes for experiments (no orderable part behind them) that take their quantities with units;
  `labFootprint(…)` gives a new one its terminals and 3D body.
- One `Laboratory` per design: the scripts by path (`world`, `output`, `modules`), the world's
  `step` and `seed`, and the starting surroundings (`temperature`, `humidity`, `co2` in `ppm`, a
  ratio unit: `600ppm`).

```pcbkit
pcbkit 2
net V5V, GND, FIRE, VIDEO
board = Board { outline = rect(120mm, 80mm) }
U1 = labComputer({ power = ["1", "2"], GP0 = ["3"], video0 = ["4"] }, firmware = "game") {
  "1" -- V5V
  "2" -- GND
  "3" -- FIRE
  "4" -- VIDEO
}
screen = labScreen(320, 200, 64mm, 40mm) { pins = [V5V, GND, VIDEO] }
fire = labButton(key = "KeyF") { pins = [V5V, GND, FIRE] }
laboratory = Laboratory {
  world = "sim/world.js"
  step = 1ms
  temperature = 30degC
}
```

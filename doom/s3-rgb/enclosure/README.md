# Enclosure

A printed case for the s3-rgb handheld: a front shell, a back shell with a grip over the two
cells, ten button caps, four M3 × 10 screws into heat-set inserts. It is PCBKit's first enclosure
(docs/adr/0030 in the pcbkit repository): `enclosure.py` is a build123d program, every place in it
comes from the board file, and `pcbkit enclosure` checks it against the board and writes `out/`.

187.8 × 92.1 × 34.7 mm (18.2 mm thick above the grip). Not printed yet: the checks pass, a first
print has to say the rest.

## Measure before printing

Three numbers are not in the board file. They are set from datasheets at the top of
`enclosure.py`; check them on the real unit with callipers and change them if yours differ by
more than 0.3 mm:

| Name | Now | What to measure |
| --- | --- | --- |
| `PANEL_TOP` | 5.95 | the panel's glass above the board's front surface (foam tape + panel) |
| `CELL_TOP` | 20.5 | the top of a cell in its holder above the board's back surface |
| `LEVER` | 1.45 | how far the on/off lever stands out of the switch's body |

Also look at the USB-C socket: its mouth should be within 1 mm of the board's top edge.

## Print

FDM, 0.4 mm nozzle, 0.2 mm layers, PETG or PLA, a bed of 200 mm or more. `out/print/plate-1.3mf`
has every part arranged on a 256 mm bed; the single files are turned the way they print:

| File | How many | Orientation | Supports |
| --- | --- | --- | --- |
| `out/print/front.stl` | 1 | face on the bed | none |
| `out/print/cap.stl` | 10 | flange on the bed | none |
| `out/print/back.stl` | 1 | grip on the bed | under the thin part of the back (touching the bed only) |

Print the front shell and one cap first: they take the least plastic and test every fit that
matters (the cap in its hole, the board between the posts, the window over the panel, the USB-C
opening). Print the back when those are right. `out/*.step` are the same parts as exact solids,
for a print service or another CAD program.

## Buy

- 4 × M3 × 10 socket head cap screws (ISO 4762).
- 4 × M3 heat-set inserts, 5.7 mm long, 4.6 mm outside (the 4.0 mm hole type).

## Assemble

1. Lay the front shell face down.
2. Press the four inserts into the posts with a soldering iron (220 °C for PETG), flush with the
   posts' ends.
3. Drop the ten caps into their holes, stems down.
4. Peel the tear tape off the panel if it is still there. Lower the board onto the posts, panel
   down: the on/off lever and the USB-C socket slide into their openings. Put the cells in, plus
   towards the + on the board.
5. Lower the back shell on: the module's antenna goes into the pocket at the top edge.
6. Four screws from the back. Stop when a head seats: more torque only pulls the insert.

RESET is the pin hole nearer the right edge seen from behind (SW12), BOOT the one nearer the
middle (SW13). On PCBKit the board's page shows these steps as a slider, from the device together
to every part laid flat.

## Change it

```
pcbkit enclosure            # checks it, writes out/
pcbkit enclosure --check    # only checks
```

## What the checks do not cover

- The printer: a moving fit is drawn with 0.3 mm of clearance a side, the board with 0.5 mm. A
  printer that prints holes tighter needs larger numbers (`fdm()` in the program).
- Strength. The posts and the 2.4 mm walls are sized by habit, not computed.
- The panel is taped by hand: the window is 1 mm larger than the picture on every side.
- Parts are drawn as their courtyards raised to their datasheet heights, not as their real shapes.

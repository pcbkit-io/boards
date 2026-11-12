"""Enclosure of the s3-rgb handheld: a front shell, a back shell with a grip over the two cells,
ten button caps, four M3 screws into heat-set inserts.

    pcbkit enclosure            checks it and writes enclosure/out/

Every place comes from the board file; what the file cannot say is in the block below. Device
frame, millimetres: x right, y up, z out of the front; the board's front surface is z = 0.
"""
from build123d import Axis, Plane, Polygon, Pos, chamfer, extrude, fillet

from pcbkit import Board, Enclosure, fdm, hardware, locate
from pcbkit.shapes import box, cyl, prism, rbox

board = Board.load()
p = fdm()
enc = Enclosure(board, p)
W, H, T = board.width, board.height, board.thickness

# ---- what the board file cannot say (measure on the real unit before printing) ----
PANEL_TOP = 5.95  # the panel's glass above the board front: 3.0 foam tape + 2.95 panel (datasheet)
CELL_TOP = 20.5  # the top of a cell in its holder above the board back
PANEL = (38.25, 6.4, 143.75, 73.6)  # the taped panel's outline (the silkscreen's)
# the panel is mounted flex up: active area 5.23 from its sides, 4.18 from its own top (the device's bottom)
ACTIVE = (PANEL[0] + 5.23, PANEL[1] + 4.18, PANEL[2] - 5.23, PANEL[1] + 4.18 + 53.86)
LEVER = 1.45  # the on/off lever stands this far out of the switch's body, and travels 1.6
USB_Z = 1.63  # the USB-C shell's axis above the board front

board.set_height("DS1", 0)  # the panel's footprint is only its connector's place
board.set_height("L1", 3.0)  # Sunlord SWPA4030S
board.add("panel", box(PANEL[0], PANEL[2], PANEL[1], PANEL[3], 0.0, PANEL_TOP))
for ref in ("BT1", "BT2"):
    holder = board[ref]
    # a protected 18650: 19 mm across, 70 long, lying along x
    board.add(f"cell in {ref}", Pos(holder.x, holder.y, -(T + CELL_TOP - 9.5)) * Plane.YZ.location * cyl(0, 0, 9.5, -35, 35))
slide = board["SW11"]
SLIDE_BODY_X = slide.x - 1.4
LEVER_TIP = SLIDE_BODY_X - LEVER
board.add("SW11 lever", box(LEVER_TIP, SLIDE_BODY_X, slide.y - 1.45, slide.y + 1.45, 0.2, 1.2))

# ---- hardware ----
screw = hardware.screw("M3x10")
insert = hardware.insert("M3")
holes = board.holes()
buttons = [board[f"SW{i}"] for i in range(1, 11)]  # the ten on the front; SW12, SW13 are reset and boot
BACK_PARTS = max(b.top for b in (board["SW12"], board["SW13"]))  # the tallest on the back but the holders

# ---- levels ----
M = p.gap + p.wall  # board edge to the outside
SPLIT = -0.8  # the shells meet at the middle of the board's edge
LAP, LAP_T = 1.5, p.wall / 2  # the front's wall goes on past the split as a skirt over the back's
FRONT_IN = round(PANEL_TOP + 0.85, 2)  # 6.8: the inside of the face
FRONT_OUT = FRONT_IN + p.wall
THIN_IN = -(T + BACK_PARTS + 0.7)  # the inside of the back, upper part
THIN_OUT = THIN_IN - p.wall
GRIP_IN = -(T + CELL_TOP + 1.0)  # the inside of the back over the cells
GRIP_OUT = GRIP_IN - p.wall
SEAT = -(T + screw.length - T - 4.2)  # under the screws' heads: 4.2 mm of thread in the insert
POST_R = 3.5

STEP_IN = max(board[r].box()[3] for r in ("BT1", "BT2")) + 0.5  # the grip's upper wall, inside
STEP_OUT = STEP_IN + p.wall
ant = board["U1"].box()  # the module's antenna passes the top edge
BUMP_IN = (ant[0] - 0.5, ant[2] + 0.5, ant[3] + 1.0)  # x0, x1, y1
BUMP_OUT = (BUMP_IN[0] - p.wall, BUMP_IN[1] + p.wall, BUMP_IN[2] + p.wall)

usb = board["J3"]
USB_OPEN = (13.4, 7.5)  # the largest plug body the standard allows, 12.35 x 6.5, and 0.5 a side

CAP_STEM_R, CAP_FLANGE_R = 3.8, 4.9
CAP_HOLE_R = CAP_STEM_R + p.fit
CAP_SOCKET, CAP_PLAY, CAP_PROUD = 0.4, 0.5, 1.6
LED_TUBE_R, LED_HOLE_R, LED_TUBE_Z = 2.5, 1.1, 1.6  # the two tubes overlap into one block
leds = [board["ledChg"], board["ledPg"]]


def below(sketch, y, round_r=0.0):
    """The part of an outline under the line y, its two new corners rounded."""
    from build123d import Rectangle

    cut = sketch & (Pos(W / 2, y - 100) * Rectangle(W + 100, 200))
    return fillet(cut.vertices().group_by(Axis.Y)[-1], round_r) if round_r else cut


def lap_ring(inner_grow, z0, z1, end_gap):
    """The outer part of the wall between two levels, left out over the antenna pocket."""
    ring = prism(board.sketch(M), z0, z1) - prism(board.sketch(inner_grow), z0 - 1, z1 + 1)
    return ring - box(BUMP_OUT[0] - end_gap, BUMP_OUT[1] + end_gap, H - 1, H + M + 1, z0 - 1, z1 + 1)


def post(x, y, r, z0, z1, back, ref):
    """A round post at a mounting hole, cut back where a part's courtyard reaches into it."""
    s = cyl(x, y, r, z0, z1)
    for part in board.parts.values():
        if part.back != back or part.ref == ref or not part.height:
            continue
        x0, y0, x1, y1 = part.box()
        if x0 < x + r and x1 > x - r and y0 < y + r and y1 > y - r:
            s -= box(x0 - 0.1, x1 + 0.1, y0 - 0.1, y1 + 0.1, min(z0, z1) - 1, max(z0, z1) + 1)
    return s


def usb_opening():
    w, h = USB_OPEN
    return rbox(usb.x - w / 2, usb.x + w / 2, H + p.gap - 0.2, H + M + 1, USB_Z - h / 2, USB_Z + h / 2, 2.0, Axis.Y)


def front_shell():
    body = prism(board.sketch(M), SPLIT, FRONT_OUT)
    body = chamfer(body.edges().group_by(Axis.Z)[-1], 1.5)
    body -= prism(board.sketch(p.gap), SPLIT - 1, FRONT_IN)
    body += lap_ring(M - LAP_T, SPLIT - LAP, SPLIT, 0.45)

    # the hood over the antenna pocket: flat under, 45 degrees over, prints without support
    x0, x1, y1 = BUMP_OUT
    y0 = H + M
    wedge = Plane.YZ.offset(x0) * Polygon((y0 - 0.5, SPLIT), (y1, SPLIT), (y1, SPLIT + 1.2), (y0 - 0.5, SPLIT + 1.2 + (y1 - y0 + 0.5)), align=None)
    body += extrude(wedge, x1 - x0)

    # posts at the four holes, with the inserts' holes
    for h in holes:
        body += post(h.x, h.y, POST_R + 0.1, 0.0, FRONT_IN + 0.2, False, h.ref)
    for h in holes:
        body -= locate((h.x, h.y, 0.0)) * insert.pocket()

    # light tubes over the two LEDs
    for led in leds:
        body += cyl(led.x, led.y, LED_TUBE_R, LED_TUBE_Z, FRONT_IN + 0.2)
    for led in leds:
        body -= cyl(led.x, led.y, LED_HOLE_R, LED_TUBE_Z - 1, FRONT_OUT + 1)

    # the window, 1 mm larger than the picture on every side (the panel is taped by hand), bevelled outside
    wx0, wy0, wx1, wy1 = ACTIVE[0] - 1, ACTIVE[1] - 1, ACTIVE[2] + 1, ACTIVE[3] + 1
    body -= rbox(wx0, wx1, wy0, wy1, FRONT_IN - 1, FRONT_OUT + 1, 1.0, Axis.Z)
    rim = [e for e in body.edges().group_by(Axis.Z)[-1] if wx0 - 0.1 <= e.center().X <= wx1 + 0.1 and wy0 - 0.1 <= e.center().Y <= wy1 + 0.1]
    body = chamfer(rim, 1.4)

    # the buttons' holes, each rim bevelled a little: this face is the first layer, which spreads
    for s in buttons:
        body -= cyl(s.x, s.y, CAP_HOLE_R, FRONT_IN - 1, FRONT_OUT + 1)
    rims = [
        e
        for e in body.edges().group_by(Axis.Z)[-1]
        if e.geom_type.name == "CIRCLE" and any(abs(e.arc_center.X - s.x) < 0.05 and abs(e.arc_center.Y - s.y) < 0.05 for s in buttons)
    ]
    body = chamfer(rims, 0.4)

    body -= usb_opening()
    body -= box(usb.x - USB_OPEN[0] / 2, usb.x + USB_OPEN[0] / 2, H + p.gap - 0.2, H + M + 1, SPLIT - LAP - 1, USB_Z)  # no sliver of skirt under it
    # the on/off lever: its slot, open towards the back so the board drops in, and a scoop for a nail
    half = 1.6 / 2 + 1.3 / 2 + 1.25
    body -= box(-M - 1, SLIDE_BODY_X - 0.1, slide.y - half, slide.y + half, SPLIT - LAP - 1, 2.4)
    body -= rbox(-M - 1, -M + 1.4, slide.y - 8, slide.y + 8, SPLIT - LAP - 3, 4.2, 1.5, Axis.X)
    return body


def back_outline(grow):
    from build123d import RectangleRounded

    x0, x1, y1 = BUMP_OUT if grow > p.gap else BUMP_IN
    bump = Pos((x0 + x1) / 2, (H - 6 + y1) / 2) * RectangleRounded(x1 - x0, y1 - (H - 6), 2.4 if grow > p.gap else 0.8)
    return board.sketch(grow) + bump


def back_shell():
    slab = prism(back_outline(M), THIN_OUT, SPLIT)
    slab = chamfer(slab.edges().group_by(Axis.Z)[0], 1.5)
    grip = prism(below(board.sketch(M), STEP_OUT, 3.0), GRIP_OUT, SPLIT)
    grip = chamfer(grip.edges().group_by(Axis.Z)[0], 2.0)
    body = slab + grip
    body -= prism(back_outline(p.gap), THIN_IN, SPLIT + 1)
    body -= prism(below(board.sketch(p.gap), STEP_IN), GRIP_IN, SPLIT + 1)
    body -= lap_ring(M - LAP_T - p.snug, SPLIT - LAP - p.snug, SPLIT + 1, 0.3)

    # posts under the four holes: the board is clamped between these and the front's
    for h in holes:
        body += post(h.x, h.y, POST_R, THIN_IN - 0.2, -T, True, h.ref)
    for h in holes:
        body -= locate((h.x, h.y, SEAT)) * screw.well()

    # sound slots over the two speakers
    for ref in ("LS1", "LS2"):
        s = board[ref]
        for i in range(-2, 3):
            body -= rbox(s.x + i * 2.6 - 0.8, s.x + i * 2.6 + 0.8, s.y - 5.5, s.y + 5.5, THIN_OUT - 1, THIN_IN + 1, 0.7, Axis.Z)
    # RESET (SW12) and BOOT (SW13) are pressed with a pin
    for ref in ("SW12", "SW13"):
        s = board[ref]
        body -= cyl(s.x, s.y, 1.5, THIN_OUT - 1, THIN_IN + 1)
    body -= usb_opening()
    return body


def cap():
    """A button's cap at the origin: a flange under the face, a stem through it, a socket over the actuator."""
    top = buttons[0].top
    c = cyl(0, 0, CAP_FLANGE_R, top - CAP_SOCKET, FRONT_IN - CAP_PLAY)
    stem = cyl(0, 0, CAP_STEM_R, FRONT_IN - CAP_PLAY - 0.1, FRONT_OUT + CAP_PROUD)
    c += fillet(stem.edges().group_by(Axis.Z)[-1], 0.8)
    return c - cyl(0, 0, 1.75 + 0.2, top - CAP_SOCKET - 1, top)


enc.part("front", front_shell(), up=(0, 0, -1))  # prints face down, no supports
enc.part("back", back_shell(), up=(0, 0, -1), supports=True)  # grip on the bed, supports under the thin part
enc.part("cap", cap(), at={s.ref: (s.x, s.y) for s in buttons}, up=(0, 0, 1), colour="#c8452c")  # flange on the bed
enc.cap("cap")
enc.use(insert, at={h.ref: (h.x, h.y, 0.0) for h in holes}, into="front", name="insert")
enc.use(screw, at={h.ref: (h.x, h.y, SEAT) for h in holes}, into="insert", name="screw")

enc.plug("J3")
enc.view("the display", *ACTIVE, PANEL_TOP)
for led in leds:
    enc.light(led.ref)
enc.press("SW12")
enc.press("SW13")
enc.clear("on/off lever", box(-M - 5, LEVER_TIP, slide.y - 1.4, slide.y + 1.4, 0.2, 1.2), "a nail reaches the on/off lever straight from the left side")

enc.step("Lay the front shell face down.", "front")
enc.step("Press the four inserts into the posts with a soldering iron (220 °C for PETG), flush with the posts' ends.", "insert")
enc.step("Drop the ten caps into their holes, stems down.", "cap")
enc.step("Lower the board onto the posts, panel down: the on/off lever and the USB-C socket slide into their openings. Then put the cells in, plus towards the + on the board.", "board")
enc.step("Lower the back shell on: the module's antenna goes into the pocket at the top edge.", "back")
enc.step("Four screws from the back. Stop when a head seats: more torque only pulls the insert.", "screw")

enc.build()

// The handheld's input: a game controller (a PS5 DualSense, or any in the standard mapping)
// presses the board's buttons, as fingers would. The keyboard works too: each button's key.
// D-pad or left stick: move and turn; R2 fire; cross use; L1/R1 strafe; options menu; circle enter.
const buttons = { fire: [7], use: [0], strafeLeft: [4], strafeRight: [5], menu: [9], enter: [1], up: [12], down: [13], left: [14], right: [15] }
const stick = { up: [1, -1], down: [1, 1], left: [0, -1], right: [0, 1] }
let last = {}

export function step(world) {
  const pad = world.gamepads[0]
  for (const [component, indices] of Object.entries(buttons)) {
    let pressed = !!pad && indices.some((i) => (pad.buttons[i] ?? 0) > 0.5)
    const axis = stick[component]
    if (pad && axis) pressed ||= (pad.axes[axis[0]] ?? 0) * axis[1] > 0.5
    // Only changes: a key held on the keyboard is not released by an idle controller.
    if (pressed !== (last[component] ?? false)) world.control(component, pressed ? 1 : 0)
    last[component] = pressed
  }
}

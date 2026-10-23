// The handheld's input in PCBKit's laboratory: a game controller (a PS5 DualSense, or any in the
// standard mapping) presses the board's buttons, as fingers would. Components by their refdes.
// D-pad or left stick: SW1-SW4; R2 fire (SW5); cross use (SW6); L1 strafe (SW7); options menu (SW8).
const buttons = { SW5: [7], SW6: [0], SW7: [4], SW8: [9], SW1: [12], SW2: [13], SW3: [14], SW4: [15] }
const stick = { SW1: [1, -1], SW2: [1, 1], SW3: [0, -1], SW4: [0, 1] }
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

// The handheld's input in PCBKit's laboratory: a game controller (a PS5 DualSense, or any in the
// standard mapping) presses the board's buttons, as fingers would. Components by their refdes.
// D-pad or left stick: SW1-SW4; R2 fire (SW7); cross use (SW8); L1/R1 strafe (SW5/SW6); options menu (SW9); circle enter (SW10).
const buttons = { SW7: [7], SW8: [0], SW5: [4], SW6: [5], SW9: [9], SW10: [1], SW1: [12], SW2: [13], SW3: [14], SW4: [15] }
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

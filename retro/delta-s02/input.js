// What a person plugs into the Delta-S02, operated from the host: the keyboard on XS2/XS3 takes
// the host's keys, a Kempston joystick on XS6 the first gamepad, a Sinclair one on XS8 the second.
// The board reads them as it would the real ones: key contacts on its matrix, joystick contacts
// on its sockets' lines.

/** A host key as the Spectrum keys it holds (a shifted one holds CAPS SHIFT or SYMBOL SHIFT too). */
const SPECIAL = {
  Enter: ["ENTER"], Space: ["SPACE"], ShiftLeft: ["CAPS"], ShiftRight: ["SYMBOL"],
  ControlLeft: ["SYMBOL"], ControlRight: ["SYMBOL"], AltLeft: ["SYMBOL"], AltRight: ["SYMBOL"],
  Backspace: ["CAPS", "0"], ArrowLeft: ["CAPS", "5"], ArrowDown: ["CAPS", "6"], ArrowUp: ["CAPS", "7"], ArrowRight: ["CAPS", "8"],
  Comma: ["SYMBOL", "N"], Period: ["SYMBOL", "M"], Quote: ["SYMBOL", "P"], Semicolon: ["SYMBOL", "O"],
  Minus: ["SYMBOL", "J"], Equal: ["SYMBOL", "L"], Slash: ["SYMBOL", "V"],
};
function spectrum(code) {
  if (/^Key[A-Z]$/.test(code)) return [code.slice(3)];
  if (/^Digit[0-9]$/.test(code)) return [code.slice(5)];
  return SPECIAL[code] ?? [];
}

/** A gamepad (standard mapping) as a joystick's buttons: the D-pad or the left stick, fire on
 * cross, circle or the right trigger. */
function joystick(pad) {
  if (!pad) return [];
  const b = (i) => (pad.buttons[i] ?? 0) > 0.5, x = pad.axes[0] ?? 0, y = pad.axes[1] ?? 0;
  const out = [];
  if (b(14) || x < -0.5) out.push("LEFT");
  if (b(15) || x > 0.5) out.push("RIGHT");
  if (b(13) || y > 0.5) out.push("DOWN");
  if (b(12) || y < -0.5) out.push("UP");
  if (b(0) || b(1) || b(7)) out.push("FIRE");
  return out;
}

/** What each component holds now, so a step presses and lets go only what changed. */
const held = { XS2: new Set(), XS6: new Set(), XS8: new Set() };
function hold(world, component, names) {
  const next = new Set(names), was = held[component], change = [];
  for (const n of next) if (!was.has(n)) change.push("+" + n);
  for (const n of was) if (!next.has(n)) change.push("-" + n);
  if (change.length) world.input(component, change.join(" "));
  held[component] = next;
}

export function step(world) {
  hold(world, "XS2", world.keys.flatMap(spectrum));
  hold(world, "XS6", joystick(world.gamepads[0]));
  hold(world, "XS8", joystick(world.gamepads[1]));
}

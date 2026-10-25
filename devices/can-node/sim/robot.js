// The robot around the node: four pneumatic pressure sensors on J5 (0.5-4.5 V ratiometric for
// 0-10 bar, the common industrial kind) and the node's own motion for the IMU (U3). The arm
// cycles a gripper: cylinder A extends while B vents, then the other way, every `period` seconds.
export const parameters = [
  {id: 'supply', kind: 'number', label: 'Supply pressure', default: 6, minimum: 0, maximum: 10, step: 0.5, unit: 'bar', group: 'Pneumatics'},
  {id: 'period', kind: 'number', label: 'Gripper cycle', default: 2, minimum: 0.5, maximum: 10, step: 0.5, unit: 's', group: 'Pneumatics'},
  {id: 'leak', kind: 'boolean', label: 'Leak on cylinder B', default: false, group: 'Pneumatics'},
  {id: 'bump', kind: 'action', label: 'Bump the robot', group: 'Motion'},
];

const volts = (bar) => 0.5 + 4 * Math.min(Math.max(bar, 0), 10) / 10;
let shock = 0;

export function step(w) {
  const p = w.parameters;
  for (const e of w.events) if (e.id === 'bump') shock = 1;
  const phase = (w.time / p.period) % 1;
  // A first-order fill and vent (time constant 0.1 s of the cycle) on each cylinder.
  const fill = (on, t) => on ? p.supply * (1 - Math.exp(-t / 0.1)) : p.supply * Math.exp(-t / 0.1);
  const t = (phase % 0.5) * p.period;
  const a = fill(phase < 0.5, t);
  const b = fill(phase >= 0.5, t) * (p.leak ? 0.6 : 1);
  w.drive('J5', 'analog', volts(p.supply));
  w.drive('J5', 'analog2', volts(a));
  w.drive('J5', 'analog3', volts(b));
  w.drive('J5', 'analog4', volts(0.02 * w.random()));
  // The arm swings with the gripper: a little rotation about Z, gravity on Z, a bump's ring-down.
  const swing = Math.sin(2 * Math.PI * w.time / p.period);
  const ring = shock * Math.sin(2 * Math.PI * 40 * w.time);
  shock *= Math.exp(-w.dt / 0.08);
  w.drive('U3', 'gyro_z_dps', 30 * swing);
  w.drive('U3', 'accel_x_g', 0.05 * swing + 0.8 * ring);
  w.drive('U3', 'accel_z_g', 1 + 0.3 * ring);
  w.measure('cylinder_a', a, 'bar');
  w.measure('cylinder_b', b, 'bar');
}

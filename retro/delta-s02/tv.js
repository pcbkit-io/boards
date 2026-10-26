// The television on the RGB socket XS9: a set of the time, its picture the one the board makes
// (the laboratory's monitor scans the board's own colour and sync lines). Shown in the laboratory's
// output; `view.screen` draws the display's latest frame.
const W = 640, H = 440;
const wood = [0.33, 0.21, 0.13, 1], bezel = [0.08, 0.08, 0.08, 1], metal = [0.62, 0.62, 0.6, 1];

export function render(view) {
  view.frame({ width: W, height: H, background: [0.11, 0.11, 0.12, 1], label: "The television on XS9 showing the Delta-S02's picture" });
  view.rect({ x: 8, y: 8, width: W - 16, height: H - 16, radius: 24, paint: { fill: wood } });
  // The tube behind its mask, 3:2 as the board's 464-dot lines fill it.
  view.rect({ x: 28, y: 28, width: 484, height: 384, radius: 34, paint: { fill: bezel } });
  view.screen({ component: "XS9", x: 40, y: 59, width: 460, height: 322 });
  // The control panel: volume and brightness, the channel keys, the power lamp.
  view.rect({ x: 528, y: 28, width: 88, height: 384, radius: 10, paint: { fill: [0.26, 0.17, 0.1, 1] } });
  for (const y of [70, 140]) {
    view.circle({ x: 572, y, radius: 22, paint: { fill: metal, stroke: [0.2, 0.2, 0.2, 1], strokeWidth: 2 } });
    view.rect({ x: 570, y: y - 20, width: 4, height: 14, paint: { fill: [0.2, 0.2, 0.2, 1] } });
  }
  for (let k = 0; k < 6; k++) {
    view.rect({ x: 546, y: 196 + k * 28, width: 52, height: 20, radius: 3, paint: { fill: k === 0 ? [0.85, 0.85, 0.8, 1] : [0.55, 0.55, 0.52, 1] } });
  }
  const on = view.state.visuals.some((v) => v.id === "XS9" && v.signal.revision > 0);
  view.circle({ x: 572, y: 380, radius: 6, paint: { fill: on ? [0.95, 0.15, 0.1, 1] : [0.3, 0.1, 0.08, 1] } });
}

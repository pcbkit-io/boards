// The house around the thermostat: one room as a single thermal mass that leaks to the outside,
// heated by the furnace while W is called and cooled by the air conditioner while Y is. The calls
// are read where the relays are driven (U5's inputs: W, Y, G). The room's temperature reaches the
// SHT40 through the laboratory's environment. Coefficients are illustrative: a 4 kW furnace and a
// 3 kW air conditioner in a room that loses 150 W per °C.
export const parameters = [
  {id: 'outside', kind: 'number', label: 'Outside', default: 5, minimum: -20, maximum: 40, step: 1, unit: '°C', group: 'Weather'},
  {id: 'start', kind: 'number', label: 'Room at start', default: 18, minimum: 5, maximum: 35, step: 0.5, unit: '°C', group: 'Room'},
  {id: 'speed', kind: 'number', label: 'Time lapse', default: 60, minimum: 1, maximum: 600, step: 1, unit: '×', group: 'Room'},
  {id: 'window', kind: 'action', label: 'Open the window for a minute', group: 'Room'},
];

const CAPACITY = 300e3;   // J/°C: the room's air and furniture
const LOSS = 150;         // W/°C to the outside
let room = null, window = 0;

const on = (w, port) => w.state.ports.find(p => p.component === 'U5' && p.port === port)?.value === 1;

export function step(w) {
  const p = w.parameters;
  if (room === null) room = p.start;
  for (const e of w.events) if (e.id === 'window') window = 60;
  const heat = on(w, 'input'), cool = on(w, 'input2'), fan = on(w, 'input3');
  const dt = w.dt * p.speed;
  const loss = (LOSS + (window > 0 ? 600 : 0)) * (room - p.outside);
  room += dt * ((heat ? 4000 : 0) - (cool ? 3000 : 0) - loss) / CAPACITY;
  window = Math.max(0, window - dt);
  w.environment({celsius: room, humidity: 45});
  w.measure('room', room, '°C');
  w.measure('furnace', heat ? 1 : 0, 'on');
  w.measure('air_conditioner', cool ? 1 : 0, 'on');
  w.measure('fan', fan ? 1 : 0, 'on');
}

# Egg catcher

The 1980s pocket game on a battery: eggs roll down four chutes and the catcher's basket goes where you press, Q and A on the left, P and L on the right; three broken eggs end it. A LiPo cell charged from its own USB-C, an on/off switch and a 5 V boost; the Nano's USB flashes it.

- Author: PCBKit
- License: CC0-1.0

This project began as the demo “Egg catcher” of PCBKit (https://pcbkit.io): https://app.pcbkit.io/demo/egg-catcher.

A battery handheld from orderable parts: an Arduino Nano on the back (flashed over its USB), a
0.96" I2C OLED, four 6 x 6 mm buttons, a LiPo cell on a JST PH header (check the cell's polarity),
an MCP73831 charging it at 500 mA from a USB-C, an AO3401A behind the slide switch and an MT3608
boost to 5 V. Placed and routed by PCBKit; the sketch builds and runs in the browser.

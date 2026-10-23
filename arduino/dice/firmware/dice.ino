// Dice for the pocket console: A rolls (the dice tumble for a moment before they settle), up and
// down choose one to five dice, B shows the sum of each roll so far as a bar chart.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum { UP = 2, DOWN = 3, BUTTON_A = 6, BUTTON_B = 7 };
uint8_t dice = 2, faces[5] = {1, 1, 1, 1, 1};
uint16_t sums[31];                    // how often each sum came up
uint16_t rolls;
bool chart;
unsigned long pressedAt;

void face(int16_t x, int16_t y, uint8_t n) {
  // A 22-pixel die with its pips.
  display.drawRoundRect(x, y, 22, 22, 4, SSD1306_WHITE);
  const int8_t c = 11, o = 6;
  auto pip = [&](int8_t dx, int8_t dy) { display.fillCircle(x + c + dx, y + c + dy, 2, SSD1306_WHITE); };
  if (n % 2) pip(0, 0);
  if (n >= 2) { pip(-o, -o); pip(o, o); }
  if (n >= 4) { pip(o, -o); pip(-o, o); }
  if (n == 6) { pip(-o, 0); pip(o, 0); }
}

void roll() {
  // Tumbling: random faces, slowing down, then the result.
  randomSeed(micros());
  for (uint8_t t = 0; t < 10; t++) {
    for (uint8_t i = 0; i < dice; i++) faces[i] = random(1, 7);
    draw();
    delay(30 + t * 12);
  }
  uint8_t sum = 0;
  for (uint8_t i = 0; i < dice; i++) sum += faces[i];
  sums[sum]++; rolls++;
}

void draw() {
  display.clearDisplay();
  if (chart) {
    display.setCursor(0, 0);
    display.print(rolls);
    display.print(F(" rolls of "));
    display.print(dice);
    uint16_t most = 1;
    for (uint8_t s = dice; s <= dice * 6; s++) most = max(most, sums[s]);
    uint8_t span = dice * 5 + 1, w = 128 / span;
    for (uint8_t s = dice; s <= dice * 6; s++) {
      uint8_t h = sums[s] * 50 / most;
      display.fillRect((s - dice) * w, 63 - h, w - 1, h + 1, SSD1306_WHITE);
    }
  } else {
    int16_t width = dice * 25 - 3, x = (128 - width) / 2;
    for (uint8_t i = 0; i < dice; i++) face(x + i * 25, 14, faces[i]);
    uint8_t sum = 0;
    for (uint8_t i = 0; i < dice; i++) sum += faces[i];
    display.setCursor(0, 0);
    display.print(dice);
    display.print(dice == 1 ? F(" die") : F(" dice"));
    display.setCursor(52, 46);
    display.setTextSize(2);
    display.print(sum);
    display.setTextSize(1);
  }
  display.display();
}

void setup() {
  for (uint8_t pin = UP; pin <= BUTTON_B; pin++) pinMode(pin, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  draw();
}

void loop() {
  if (millis() - pressedAt < 250) return;
  if (digitalRead(BUTTON_A) == LOW) { pressedAt = millis(); chart = false; roll(); }
  else if (digitalRead(UP) == LOW && dice < 5) { pressedAt = millis(); dice++; memset(sums, 0, sizeof sums); rolls = 0; }
  else if (digitalRead(DOWN) == LOW && dice > 1) { pressedAt = millis(); dice--; memset(sums, 0, sizeof sums); rolls = 0; }
  else if (digitalRead(BUTTON_B) == LOW) { pressedAt = millis(); chart = !chart; }
  else return;
  draw();
}

// A flappy bird for the pocket console: A flaps; gravity pulls; pipes scroll in from the right
// with a gap to fly through. One point a pipe; the best stays in the EEPROM.
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

const uint8_t BUTTON_A = 6, PIPES = 3, GAP = 22, WIDTH = 10;
// The bird: 8 x 6 pixels, wings up and down.
const uint8_t PROGMEM wingsUp[] = {0x3c, 0x5e, 0xff, 0xfe, 0x7c, 0x38};
const uint8_t PROGMEM wingsDown[] = {0x3c, 0x7e, 0xfb, 0xfe, 0x7c, 0x10};
int16_t birdY, velocity;            // 1/16 pixel
int16_t pipeX[PIPES];
uint8_t gapY[PIPES];
uint16_t score, best;
bool alive, started, held;

void reset() {
  birdY = 28 * 16; velocity = 0; score = 0; alive = true; started = false;
  for (uint8_t i = 0; i < PIPES; i++) { pipeX[i] = 128 + i * 48; gapY[i] = random(12, 64 - GAP - 4); }
}

void update() {
  bool button = digitalRead(BUTTON_A) == LOW;
  if (button && !held) {
    if (!alive) reset();
    else { started = true; velocity = -38; }
  }
  held = button;
  if (!alive || !started) return;
  velocity = min(velocity + 3, 60);
  birdY += velocity;
  if (birdY < 0) { birdY = 0; velocity = 0; }
  for (uint8_t i = 0; i < PIPES; i++) {
    pipeX[i]--;
    if (pipeX[i] == 20 - WIDTH) score++;
    if (pipeX[i] < -WIDTH) { pipeX[i] += PIPES * 48; gapY[i] = random(12, 64 - GAP - 4); }
    bool overlaps = pipeX[i] < 28 && pipeX[i] + WIDTH > 20;
    int16_t y = birdY / 16;
    if (overlaps && (y < gapY[i] || y + 6 > gapY[i] + GAP)) alive = false;
  }
  if (birdY / 16 > 57) alive = false;
  if (!alive && score > best) { best = score; EEPROM.put(0, best); }
}

void draw() {
  display.clearDisplay();
  for (uint8_t i = 0; i < PIPES; i++) {
    display.fillRect(pipeX[i], 0, WIDTH, gapY[i], SSD1306_WHITE);
    display.fillRect(pipeX[i], gapY[i] + GAP, WIDTH, 64 - gapY[i] - GAP, SSD1306_WHITE);
    display.drawFastHLine(pipeX[i] - 1, gapY[i] - 1, WIDTH + 2, SSD1306_WHITE);
    display.drawFastHLine(pipeX[i] - 1, gapY[i] + GAP, WIDTH + 2, SSD1306_WHITE);
  }
  display.drawBitmap(20, birdY / 16, velocity < 0 ? wingsDown : wingsUp, 8, 6, SSD1306_WHITE);
  display.fillRect(0, 0, 20, 9, SSD1306_BLACK);
  display.setCursor(1, 1);
  display.print(score);
  if (!started || !alive) {
    display.fillRect(30, 18, 68, 30, SSD1306_BLACK);
    display.drawRect(30, 18, 68, 30, SSD1306_WHITE);
    display.setCursor(36, 23);
    display.print(alive ? F("FLAPPY") : F("CRASH!"));
    display.setCursor(36, 35);
    display.print(F("best "));
    display.print(best);
  }
  display.display();
}

void setup() {
  pinMode(BUTTON_A, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  EEPROM.get(0, best);
  if (best == 0xffff) best = 0;
  randomSeed(micros());
  reset();
}

void loop() {
  static unsigned long frameAt;
  if (millis() - frameAt < 25) return;
  frameAt = millis();
  update();
  draw();
}

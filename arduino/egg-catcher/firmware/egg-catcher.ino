// Egg catcher: eggs roll down four chutes, two on each side of the screen, one step per tick; the
// catcher's basket waits at the chute whose button was pressed last. An egg that reaches the end
// of its chute lands in the basket (a point) or breaks on the ground (a miss); three misses end
// the game. The game speeds up as the score grows; the best score is kept in the EEPROM.
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

// The buttons, one per chute: left top, left bottom, right top, right bottom.
const uint8_t BUTTONS[4] = {2, 3, 4, 5};
const uint8_t STEPS = 5;                  // positions along a chute; the last one is its end
const uint8_t MAX_EGGS = 8;

struct Egg { uint8_t chute, step; bool live; };
Egg eggs[MAX_EGGS];
uint8_t basket, misses, brokenChute, brokenFor;
uint16_t score, best;
bool over;
unsigned long tickAt, overAt;
uint8_t ticksSinceEgg;

// A chute runs from the screen's edge towards the middle and down: left ones left to right.
int chuteX(uint8_t chute, uint8_t step) { int x = 4 + step * 7; return chute < 2 ? x : 127 - x; }
int chuteY(uint8_t chute, uint8_t step) { return (chute % 2 ? 36 : 16) + step * 3; }
uint16_t period() { return max(180, 520 - (int)score * 6); }

void start() {
  for (Egg &e : eggs) e.live = false;
  score = 0; misses = 0; brokenFor = 0; over = false; basket = 0; ticksSinceEgg = 0;
  tickAt = millis();
}

void spawn() {
  // One egg at a time on a chute's top step, never two arriving on the same tick.
  uint8_t chute = random(4);
  for (Egg &e : eggs) if (e.live && e.step == 0) return;
  for (Egg &e : eggs) if (!e.live) { e = {chute, 0, true}; ticksSinceEgg = 0; return; }
}

void tick() {
  for (Egg &e : eggs) {
    if (!e.live) continue;
    if (e.step + 1 < STEPS) { e.step++; continue; }
    e.live = false;
    if (e.chute == basket) score++;
    else { misses++; brokenChute = e.chute; brokenFor = 4; }
  }
  if (brokenFor) brokenFor--;
  // More eggs as the score grows: every 3 ticks at first, every tick at 40 points.
  uint8_t gap = score < 15 ? 3 : score < 40 ? 2 : 1;
  if (++ticksSinceEgg >= gap && random(3)) spawn();
  if (misses >= 3) {
    over = true;
    overAt = millis();
    if (score > best) { best = score; EEPROM.put(0, best); }
  }
}

void drawChutes() {
  for (uint8_t c = 0; c < 4; c++) {
    int x0 = chuteX(c, 0), y0 = chuteY(c, 0) + 3, x1 = chuteX(c, STEPS - 1), y1 = chuteY(c, STEPS - 1) + 3;
    display.drawLine(x0 + (c < 2 ? -4 : 4), y0, x1, y1, SSD1306_WHITE);
  }
}

void drawEgg(int x, int y) { display.fillRoundRect(x - 2, y - 3, 5, 6, 2, SSD1306_WHITE); }

void drawCatcher() {
  // The body in the middle, the arms and the basket towards the chute it guards.
  bool right = basket >= 2, low = basket % 2;
  int cx = 64;
  int side = right ? 1 : -1;
  display.drawCircle(cx, 26, 6, SSD1306_WHITE);                       // head
  display.fillTriangle(cx - 5, 22, cx - 2, 20, cx - 5, 14, SSD1306_WHITE);   // pointed ears
  display.fillTriangle(cx + 5, 22, cx + 2, 20, cx + 5, 14, SSD1306_WHITE);
  display.drawTriangle(cx + side * 5, 24, cx + side * 5, 29, cx + side * 11, 28, SSD1306_WHITE);  // the snout
  display.drawPixel(cx + side * 2, 24, SSD1306_WHITE);                // the eye, towards the basket
  display.drawRoundRect(cx - 7, 33, 15, 18, 4, SSD1306_WHITE);        // body
  display.drawLine(cx - 4, 51, cx - 6, 62, SSD1306_WHITE);           // legs
  display.drawLine(cx + 4, 51, cx + 6, 62, SSD1306_WHITE);
  int hx = right ? cx + 22 : cx - 22, hy = low ? 46 : 30;
  display.drawLine(cx + (right ? 6 : -6), 37, hx, hy, SSD1306_WHITE);  // the arm
  display.drawLine(hx - 6, hy, hx + 6, hy, SSD1306_WHITE);             // the basket
  display.drawLine(hx - 6, hy, hx - 4, hy + 5, SSD1306_WHITE);
  display.drawLine(hx + 6, hy, hx + 4, hy + 5, SSD1306_WHITE);
  display.drawLine(hx - 4, hy + 5, hx + 4, hy + 5, SSD1306_WHITE);
}

void draw() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(48, 0);
  display.print(score);
  for (uint8_t i = 0; i < misses; i++) drawEgg(102 + i * 9, 4);       // the broken ones so far
  drawChutes();
  for (Egg &e : eggs) if (e.live) drawEgg(chuteX(e.chute, e.step), chuteY(e.chute, e.step));
  if (brokenFor) {
    int x = brokenChute < 2 ? 30 : 97;
    display.drawLine(x - 5, 61, x - 1, 57, SSD1306_WHITE);            // the shell on the ground
    display.drawLine(x + 5, 61, x + 1, 57, SSD1306_WHITE);
    display.drawLine(x - 6, 62, x + 6, 62, SSD1306_WHITE);
  }
  drawCatcher();
  if (over) {
    display.fillRect(20, 18, 88, 30, SSD1306_BLACK);
    display.drawRect(20, 18, 88, 30, SSD1306_WHITE);
    display.setCursor(37, 23);
    display.print(F("GAME OVER"));
    display.setCursor(30, 35);
    display.print(F("best "));
    display.print(best);
  }
  display.display();
}

void setup() {
  for (uint8_t b : BUTTONS) pinMode(b, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  EEPROM.get(0, best);
  if (best == 0xffff) best = 0;
  // A different game each time: the time the first press takes is in the seed (see loop).
  start();
}

void loop() {
  for (uint8_t i = 0; i < 4; i++) {
    if (digitalRead(BUTTONS[i]) == LOW) {
      // After a game, a press a second later starts the next (not the press that lost it).
      if (over) { if (millis() - overAt > 1000) start(); return; }
      if (basket != i) randomSeed(micros());
      basket = i;
    }
  }
  if (!over && millis() - tickAt >= period()) {
    tickAt += period();
    tick();
  }
  draw();
}

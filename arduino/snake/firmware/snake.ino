// Snake for the pocket console: a 32 x 14 field of 4-pixel cells under a score line. The D-pad
// steers, A pauses; the snake speeds up as it grows; the best score is kept in the EEPROM.
// The body lives in the field itself: each cell holds whether it is snake and the way to the next
// segment (half a byte per cell, 224 bytes), so the length costs no memory.
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum { UP = 2, DOWN = 3, LEFT = 4, RIGHT = 5, BUTTON_A = 6, BUTTON_B = 7 };
const uint8_t COLS = 32, ROWS = 14, CELL = 4, TOP = 8;
uint8_t field[COLS * ROWS / 2];      // per cell: bit 2 snake, bits 0-1 direction to the next
uint8_t headX, headY, tailX, tailY, foodX, foodY, direction, nextDirection;
uint16_t length, score, best;
bool playing, paused, over;
unsigned long stepAt, pressedAt;

const int8_t DX[] = {0, 1, 0, -1}, DY[] = {-1, 0, 1, 0};   // up, right, down, left

uint8_t cell(uint8_t x, uint8_t y) { uint16_t i = y * COLS + x; return i & 1 ? field[i / 2] >> 4 : field[i / 2] & 15; }
void setCell(uint8_t x, uint8_t y, uint8_t v) {
  uint16_t i = y * COLS + x;
  field[i / 2] = i & 1 ? (field[i / 2] & 0x0f) | (v << 4) : (field[i / 2] & 0xf0) | v;
}

void placeFood() {
  do { foodX = random(COLS); foodY = random(ROWS); } while (cell(foodX, foodY) & 4);
}

void start() {
  memset(field, 0, sizeof field);
  tailX = 10; tailY = 7; headX = 13; headY = 7; length = 4; score = 0;
  for (uint8_t x = tailX; x <= headX; x++) setCell(x, 7, 4 | 1);
  direction = nextDirection = 1;
  placeFood();
  playing = true; over = false; paused = false;
}

void step() {
  direction = nextDirection;
  setCell(headX, headY, 4 | direction);
  int8_t x = headX + DX[direction], y = headY + DY[direction];
  // The walls wrap: the field is a torus.
  x = (x + COLS) % COLS; y = (y + ROWS) % ROWS;
  bool eats = x == foodX && y == foodY;
  if (!eats) {
    uint8_t d = cell(tailX, tailY) & 3;
    setCell(tailX, tailY, 0);
    tailX = (tailX + DX[d] + COLS) % COLS; tailY = (tailY + DY[d] + ROWS) % ROWS;
  }
  if (cell(x, y) & 4) {
    over = true; playing = false;
    if (score > best) { best = score; EEPROM.put(0, best); }
    return;
  }
  headX = x; headY = y;
  setCell(headX, headY, 4 | direction);
  if (eats) { length++; score += 10; placeFood(); }
}

void draw() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SCORE "));
  display.print(score);
  display.setCursor(80, 0);
  display.print(F("BEST "));
  display.print(best);
  display.drawFastHLine(0, TOP - 1, 128, SSD1306_WHITE);
  for (uint8_t y = 0; y < ROWS; y++)
    for (uint8_t x = 0; x < COLS; x++)
      if (cell(x, y) & 4) display.fillRect(x * CELL, TOP + y * CELL, CELL - 1, CELL - 1, SSD1306_WHITE);
  display.drawRoundRect(foodX * CELL, TOP + foodY * CELL, CELL, CELL, 1, SSD1306_WHITE);
  if (!playing || paused) {
    display.fillRect(20, 22, 88, 26, SSD1306_BLACK);
    display.drawRect(20, 22, 88, 26, SSD1306_WHITE);
    display.setCursor(28, 27);
    display.print(over ? F("GAME OVER") : paused ? F("PAUSED") : F("SNAKE"));
    display.setCursor(28, 38);
    display.print(F("press A"));
  }
  display.display();
}

bool pressed(uint8_t pin) { return digitalRead(pin) == LOW; }

void setup() {
  for (uint8_t pin = UP; pin <= BUTTON_B; pin++) pinMode(pin, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  EEPROM.get(0, best);
  if (best == 0xffff) best = 0;
  memset(field, 0, sizeof field);
}

void loop() {
  if (pressed(UP) && direction != 2) nextDirection = 0;
  if (pressed(RIGHT) && direction != 3) nextDirection = 1;
  if (pressed(DOWN) && direction != 0) nextDirection = 2;
  if (pressed(LEFT) && direction != 1) nextDirection = 3;
  if (pressed(BUTTON_A) && millis() - pressedAt > 300) {
    pressedAt = millis();
    if (!playing) { randomSeed(micros()); start(); } else paused = !paused;
  }
  // Faster as it grows: 160 ms a step, down to 60.
  uint16_t interval = max(60, 160 - (int)length * 3);
  if (playing && !paused && millis() - stepAt >= interval) { stepAt = millis(); step(); }
  draw();
}

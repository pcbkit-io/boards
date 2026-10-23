// Falling blocks for the pocket console: a 10 x 20 well of 3-pixel cells in the middle of the
// screen, the next piece and the score beside it. Left and right move, A turns, down drops
// faster, B drops at once. Full lines clear; every ten lines the pace rises.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum { DOWN = 3, LEFT = 4, RIGHT = 5, BUTTON_A = 6, BUTTON_B = 7 };
const uint8_t W = 10, H = 20, CELL = 3, X0 = 49, Y0 = 2;
uint16_t well[H];                    // bit x: a block in column x
// The seven pieces as 4 x 4 masks (row-major, bit 15 top left), turned by rotate().
const uint16_t PROGMEM PIECES[7] = {0x0f00, 0x4460, 0x2260, 0x0660, 0x0c60, 0x06c0, 0x0e40};
uint16_t piece, next;
int8_t px, py;
uint16_t score, lines;
bool over;
unsigned long fallAt, moveAt;

bool block(uint16_t p, uint8_t x, uint8_t y) { return p & (0x8000 >> (y * 4 + x)); }
uint16_t rotate(uint16_t p) {
  uint16_t r = 0;
  for (uint8_t y = 0; y < 4; y++) for (uint8_t x = 0; x < 4; x++) if (block(p, x, y)) r |= 0x8000 >> (x * 4 + (3 - y));
  return r;
}
bool fits(uint16_t p, int8_t ox, int8_t oy) {
  for (uint8_t y = 0; y < 4; y++) for (uint8_t x = 0; x < 4; x++) if (block(p, x, y)) {
    int8_t wx = ox + x, wy = oy + y;
    if (wx < 0 || wx >= W || wy >= H) return false;
    if (wy >= 0 && well[wy] & (1 << wx)) return false;
  }
  return true;
}
uint16_t randomPiece() { return pgm_read_word(&PIECES[random(7)]); }
void spawn() {
  piece = next; next = randomPiece(); px = 3; py = -1;
  if (!fits(piece, px, py)) over = true;
}
void lock() {
  for (uint8_t y = 0; y < 4; y++) for (uint8_t x = 0; x < 4; x++) if (block(piece, x, y) && py + y >= 0) well[py + y] |= 1 << (px + x);
  uint8_t cleared = 0;
  for (int8_t y = H - 1; y >= 0; y--) {
    if (well[y] == (1 << W) - 1) {
      for (int8_t k = y; k > 0; k--) well[k] = well[k - 1];
      well[0] = 0; cleared++; y++;
    }
  }
  const uint16_t points[] = {0, 40, 100, 300, 1200};
  score += points[cleared]; lines += cleared;
  spawn();
}
void start() {
  memset(well, 0, sizeof well);
  score = lines = 0; over = false;
  next = randomPiece(); spawn();
}

void drawPiece(uint16_t p, int16_t ox, int16_t oy) {
  for (uint8_t y = 0; y < 4; y++) for (uint8_t x = 0; x < 4; x++)
    if (block(p, x, y) && oy + y * CELL >= Y0) display.fillRect(ox + x * CELL, oy + y * CELL, CELL - 1, CELL - 1, SSD1306_WHITE);
}
void draw() {
  display.clearDisplay();
  display.drawRect(X0 - 2, Y0 - 2, W * CELL + 3, H * CELL + 3, SSD1306_WHITE);
  for (uint8_t y = 0; y < H; y++) for (uint8_t x = 0; x < W; x++)
    if (well[y] & (1 << x)) display.fillRect(X0 + x * CELL, Y0 + y * CELL, CELL - 1, CELL - 1, SSD1306_WHITE);
  drawPiece(piece, X0 + px * CELL, Y0 + py * CELL);
  display.setCursor(86, 2); display.print(F("NEXT"));
  drawPiece(next, 88, 12);
  display.setCursor(2, 2); display.print(F("SCORE"));
  display.setCursor(2, 12); display.print(score);
  display.setCursor(2, 28); display.print(F("LINES"));
  display.setCursor(2, 38); display.print(lines);
  if (over) { display.fillRect(36, 26, 56, 12, SSD1306_BLACK); display.setCursor(40, 28); display.print(F("GAME OVER")); }
  display.display();
}

bool pressed(uint8_t pin) { return digitalRead(pin) == LOW; }

void setup() {
  for (uint8_t pin = DOWN; pin <= BUTTON_B; pin++) pinMode(pin, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  randomSeed(micros());
  start();
}

void loop() {
  if (over) { if (pressed(BUTTON_A)) { delay(300); start(); } draw(); return; }
  if (millis() - moveAt > 120) {
    if (pressed(LEFT) && fits(piece, px - 1, py)) { px--; moveAt = millis(); }
    if (pressed(RIGHT) && fits(piece, px + 1, py)) { px++; moveAt = millis(); }
    if (pressed(BUTTON_A)) { uint16_t r = rotate(piece); if (fits(r, px, py)) piece = r; moveAt = millis() + 80; }
    if (pressed(BUTTON_B)) { while (fits(piece, px, py + 1)) py++; lock(); moveAt = millis() + 150; }
  }
  uint16_t interval = pressed(DOWN) ? 40 : max(100, 600 - (int)(lines / 10) * 60);
  if (millis() - fallAt > interval) {
    fallAt = millis();
    if (fits(piece, px, py + 1)) py++; else lock();
  }
  draw();
}

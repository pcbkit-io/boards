// Breakout for the pocket console: left and right move the paddle, A serves. Five rows of bricks
// (one bit each), three balls; the ball's angle off the paddle depends on where it lands.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum { LEFT = 4, RIGHT = 5, BUTTON_A = 6 };
const uint8_t ROWS = 5, COLUMNS = 16, BRICK_W = 8, BRICK_H = 4, TOP = 10, PADDLE = 18;
uint16_t bricks[ROWS];               // bit c: the brick in column c stands
int16_t ballX, ballY, speedX, speedY; // 1/16 pixel
int16_t paddle = 55;
uint8_t balls = 3;
uint16_t score;
bool attached = true;

void level() { for (uint8_t r = 0; r < ROWS; r++) bricks[r] = 0xffff; }

void update() {
  if (digitalRead(LEFT) == LOW) paddle -= 3;
  if (digitalRead(RIGHT) == LOW) paddle += 3;
  paddle = constrain(paddle, 0, 128 - PADDLE);
  if (attached) {
    ballX = (paddle + PADDLE / 2) * 16; ballY = 58 * 16;
    if (digitalRead(BUTTON_A) == LOW && balls) { attached = false; speedX = random(2) ? 14 : -14; speedY = -22; }
    return;
  }
  ballX += speedX; ballY += speedY;
  int16_t x = ballX / 16, y = ballY / 16;
  if (x <= 0 || x >= 127) { speedX = -speedX; ballX = constrain(ballX, 16, 126 * 16); }
  if (y <= TOP - 1) { speedY = -speedY; ballY = TOP * 16; }
  // A brick under the ball breaks and turns it.
  int8_t row = (y - TOP - 2) / BRICK_H, column = x / BRICK_W;
  if (y >= TOP + 2 && row < ROWS && column < COLUMNS && bricks[row] & (1u << column)) {
    bricks[row] &= ~(1u << column);
    speedY = -speedY;
    score += (ROWS - row) * 10;
    bool cleared = true;
    for (uint8_t r = 0; r < ROWS; r++) if (bricks[r]) cleared = false;
    if (cleared) { level(); attached = true; }
  }
  if (speedY > 0 && y >= 59 && y <= 61 && x >= paddle - 1 && x <= paddle + PADDLE) {
    int16_t offset = x - paddle - PADDLE / 2;
    speedX = offset * 3; speedY = -speedY;
  }
  if (y > 64) { balls--; attached = true; }
}

void draw() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(score);
  for (uint8_t b = 0; b < balls; b++) display.fillCircle(124 - b * 6, 3, 2, SSD1306_WHITE);
  for (uint8_t r = 0; r < ROWS; r++)
    for (uint8_t c = 0; c < COLUMNS; c++)
      if (bricks[r] & (1u << c)) display.fillRect(c * BRICK_W, TOP + 2 + r * BRICK_H, BRICK_W - 1, BRICK_H - 1, SSD1306_WHITE);
  display.fillRect(paddle, 61, PADDLE, 2, SSD1306_WHITE);
  display.fillRect(ballX / 16 - 1, ballY / 16 - 1, 3, 3, SSD1306_WHITE);
  if (!balls) {
    display.fillRect(34, 32, 60, 14, SSD1306_BLACK);
    display.setCursor(38, 35);
    display.print(F("GAME OVER"));
  }
  display.display();
}

void setup() {
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_A, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
  randomSeed(micros());
  level();
}

void loop() {
  static unsigned long frameAt;
  if (millis() - frameAt < 20) return;
  frameAt = millis();
  if (!balls && digitalRead(BUTTON_A) == LOW) { balls = 3; score = 0; level(); delay(300); }
  update();
  draw();
}

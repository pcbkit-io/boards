// Pong for the pocket console: you on the left (up and down), the computer on the right. The ball
// speeds up with every return and leaves the paddle at an angle that depends on where it hit;
// first to seven wins. A serves.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

enum { UP = 2, DOWN = 3, BUTTON_A = 6 };
const int8_t PADDLE = 14, WIN = 7;
// Positions in 1/16 pixel, for smooth motion at 50 frames a second.
int16_t ballX, ballY, speedX, speedY;
int16_t player = 25 * 16, computer = 25 * 16;
uint8_t playerScore, computerScore;
bool serving = true;

void serve(int8_t toward) {
  ballX = 64 * 16; ballY = 32 * 16;
  speedX = toward * 24; speedY = random(-16, 17);
  serving = false;
}

void bounce(int16_t paddle) {
  // Where on the paddle it hit sets the angle; each return is a little faster.
  int16_t offset = (ballY - paddle) / 16 - PADDLE / 2;
  speedY = offset * 6;
  speedX = -speedX + (speedX > 0 ? -2 : 2);
  speedX = constrain(speedX, -72, 72);
}

void update() {
  if (digitalRead(UP) == LOW) player -= 40;
  if (digitalRead(DOWN) == LOW) player += 40;
  player = constrain(player, 9 * 16, (63 - PADDLE) * 16);
  // The computer follows the ball, at a limited speed: it can be beaten.
  int16_t target = ballY - PADDLE * 8;
  computer += constrain(target - computer, -26, 26);
  computer = constrain(computer, 9 * 16, (63 - PADDLE) * 16);
  if (serving) return;
  ballX += speedX; ballY += speedY;
  if (ballY < 9 * 16 || ballY > 62 * 16) { speedY = -speedY; ballY = constrain(ballY, 9 * 16, 62 * 16); }
  if (speedX < 0 && ballX <= 4 * 16 && ballX > 1 * 16 && ballY >= player && ballY <= player + PADDLE * 16) bounce(player);
  if (speedX > 0 && ballX >= 123 * 16 && ballX < 126 * 16 && ballY >= computer && ballY <= computer + PADDLE * 16) bounce(computer);
  if (ballX < 0) { computerScore++; serving = true; }
  if (ballX > 128 * 16) { playerScore++; serving = true; }
}

void draw() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(52, 0);
  display.print(playerScore);
  display.print(F(" : "));
  display.print(computerScore);
  display.drawFastHLine(0, 8, 128, SSD1306_WHITE);
  for (uint8_t y = 10; y < 64; y += 6) display.drawFastVLine(64, y, 3, SSD1306_WHITE);
  display.fillRect(2, player / 16, 2, PADDLE, SSD1306_WHITE);
  display.fillRect(124, computer / 16, 2, PADDLE, SSD1306_WHITE);
  if (!serving) display.fillRect(ballX / 16 - 1, ballY / 16 - 1, 3, 3, SSD1306_WHITE);
  if (serving) {
    display.setCursor(playerScore == WIN || computerScore == WIN ? 34 : 40, 30);
    if (playerScore == WIN) display.print(F("YOU WIN!"));
    else if (computerScore == WIN) display.print(F("YOU LOSE"));
    else display.print(F("A: SERVE"));
  }
  display.display();
}

void setup() {
  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(BUTTON_A, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  static unsigned long frameAt;
  if (millis() - frameAt < 20) return;
  frameAt = millis();
  if (serving && digitalRead(BUTTON_A) == LOW) {
    if (playerScore == WIN || computerScore == WIN) playerScore = computerScore = 0;
    randomSeed(micros());
    serve(random(2) ? 1 : -1);
  }
  update();
  draw();
}

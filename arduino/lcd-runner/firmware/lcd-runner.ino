// A runner on a 16 x 2 LCD (LiquidCrystal, 4-bit): the dinosaur in column 1 jumps (button on D8,
// INPUT_PULLUP) over cacti on the ground and ducks under nothing: birds fly on the top line and
// must be run under. Characters 0-5 of the LCD's CGRAM are the sprites; the world scrolls one
// column a step, faster as the score grows.
#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
const uint8_t JUMP = 8;

byte runA[8] = {0b00111, 0b00101, 0b00111, 0b10110, 0b11111, 0b01110, 0b01010, 0b10001};
byte runB[8] = {0b00111, 0b00101, 0b00111, 0b10110, 0b11111, 0b01110, 0b01010, 0b01010};
byte jumping[8] = {0b00111, 0b00101, 0b10111, 0b11110, 0b01110, 0b01010, 0b00000, 0b00000};
byte cactus[8] = {0b00100, 0b00101, 0b10101, 0b10101, 0b10111, 0b11100, 0b00100, 0b00100};
byte bird[8] = {0b00000, 0b01000, 0b11000, 0b01111, 0b00110, 0b00100, 0b00000, 0b00000};
byte cloud[8] = {0b00000, 0b00000, 0b01100, 0b11110, 0b11111, 0b00000, 0b00000, 0b00000};
enum { RUN_A, RUN_B, JUMPING, CACTUS, BIRD, CLOUD };

char ground[17], sky[17];
uint8_t airborne;           // steps left in the air
uint16_t score, best;
bool over = true;
unsigned long stepAt;

void reset() {
  memset(ground, ' ', 16); memset(sky, ' ', 16); ground[16] = sky[16] = 0;
  airborne = 0; score = 0; over = false;
}

void draw() {
  lcd.setCursor(0, 0);
  for (uint8_t c = 0; c < 16; c++) lcd.write(c == 1 && airborne ? (uint8_t)JUMPING : sky[c] == 'B' ? (uint8_t)BIRD : sky[c] == 'c' ? (uint8_t)CLOUD : (uint8_t)' ');
  lcd.setCursor(0, 1);
  for (uint8_t c = 0; c < 16; c++) lcd.write(c == 1 && !airborne ? (uint8_t)(score % 2 ? RUN_A : RUN_B) : ground[c] == 'C' ? (uint8_t)CACTUS : (uint8_t)' ');
  lcd.setCursor(11, 0);
  if (sky[11] == ' ' && sky[12] == ' ') { char text[6]; snprintf(text, sizeof text, "%5u", score); lcd.print(text); }
}

void step() {
  memmove(ground, ground + 1, 15); memmove(sky, sky + 1, 15);
  ground[15] = ' '; sky[15] = ' ';
  // An obstacle now and then, never two too close to be passed.
  bool room = ground[13] == ' ' && ground[14] == ' ' && sky[14] == ' ';
  long r = random(100);
  if (room && r < 18) ground[15] = 'C';
  else if (room && r < 26 && score > 30) sky[15] = 'B';
  else if (r > 94) sky[15] = 'c';
  if (airborne) airborne--;
  if ((ground[1] == 'C' && !airborne) || (sky[1] == 'B' && airborne)) {
    over = true;
    if (score > best) best = score;
    return;
  }
  score++;
}

void setup() {
  pinMode(JUMP, INPUT_PULLUP);
  lcd.begin(16, 2);
  lcd.createChar(RUN_A, runA); lcd.createChar(RUN_B, runB); lcd.createChar(JUMPING, jumping);
  lcd.createChar(CACTUS, cactus); lcd.createChar(BIRD, bird); lcd.createChar(CLOUD, cloud);
  lcd.clear();
  lcd.print(F("  LCD  RUNNER"));
  lcd.setCursor(0, 1);
  lcd.print(F(" press to jump"));
}

void loop() {
  bool pressed = digitalRead(JUMP) == LOW;
  if (over) {
    if (pressed) { randomSeed(micros()); reset(); lcd.clear(); while (digitalRead(JUMP) == LOW) delay(5); }
    return;
  }
  if (pressed && !airborne) airborne = 3;
  uint16_t interval = max(90, 260 - (int)score);
  if (millis() - stepAt >= interval) {
    stepAt = millis();
    step();
    draw();
    if (over) {
      lcd.clear();
      lcd.print(F("GAME OVER "));
      lcd.print(score);
      lcd.setCursor(0, 1);
      lcd.print(F("best "));
      lcd.print(best);
      delay(600);
    }
  }
}

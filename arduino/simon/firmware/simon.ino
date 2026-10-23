// Simon: four lights (D6-D9) and their buttons (D2-D5), the round and the best on the OLED. The
// sequence grows by one each round and plays a little faster; a wrong button ends the game and the
// lights spin. The best round is kept in the EEPROM.
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

const uint8_t BUTTONS[4] = {2, 3, 4, 5}, LIGHTS[4] = {6, 7, 8, 9};
const uint8_t LONGEST = 64;
uint8_t sequence[LONGEST], round_, best;

void show(const __FlashStringHelper *line, uint8_t value) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SIMON      best "));
  display.print(best);
  display.setTextSize(2);
  display.setCursor(0, 24);
  display.print(line);
  display.print(value);
  display.display();
}

void flash(uint8_t i, uint16_t ms) {
  digitalWrite(LIGHTS[i], HIGH);
  delay(ms);
  digitalWrite(LIGHTS[i], LOW);
}

int8_t button() {
  for (uint8_t i = 0; i < 4; i++) if (digitalRead(BUTTONS[i]) == LOW) return i;
  return -1;
}

// The next button pressed (lit while held), or -1 after five seconds.
int8_t press() {
  unsigned long start = millis();
  while (millis() - start < 5000) {
    int8_t b = button();
    if (b >= 0) {
      digitalWrite(LIGHTS[b], HIGH);
      while (button() == b) delay(5);
      digitalWrite(LIGHTS[b], LOW);
      delay(40);
      return b;
    }
  }
  return -1;
}

void lose() {
  show(F("over: "), round_ - 1);
  for (uint8_t k = 0; k < 12; k++) flash(k % 4, 60);
  if (round_ - 1 > best) { best = round_ - 1; EEPROM.update(0, best); }
  round_ = 0;
}

void setup() {
  for (uint8_t i = 0; i < 4; i++) { pinMode(BUTTONS[i], INPUT_PULLUP); pinMode(LIGHTS[i], OUTPUT); }
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  best = EEPROM.read(0);
  if (best == 0xff) best = 0;
}

void loop() {
  if (round_ == 0) {
    show(F("press any"), 0);
    while (button() < 0) delay(5);
    randomSeed(micros());
    while (button() >= 0) delay(5);
    delay(300);
  }
  sequence[round_++] = random(4);
  show(F("round "), round_);
  uint16_t pace = max(150, 450 - round_ * 15);
  for (uint8_t i = 0; i < round_; i++) { flash(sequence[i], pace); delay(pace / 2); }
  for (uint8_t i = 0; i < round_; i++) {
    if (press() != sequence[i]) { lose(); return; }
  }
  if (round_ == LONGEST) { show(F("you win "), round_); round_ = 0; }
  delay(600);
}

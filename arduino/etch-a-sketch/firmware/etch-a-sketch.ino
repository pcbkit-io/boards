// A magic drawing screen: A0 and A1 are the knobs (10 kΩ pots), D2 wipes. The pen draws a line
// from where it was to where the knobs say, so a quick turn still leaves a continuous stroke; the
// reading is averaged over eight samples to steady the pen.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

const uint8_t KNOB_X = A0, KNOB_Y = A1, WIPE = 2;
int16_t penX = -1, penY = -1;

int16_t knob(uint8_t pin, int16_t size) {
  long sum = 0;
  for (uint8_t i = 0; i < 8; i++) sum += analogRead(pin);
  return (sum / 8) * (size - 1) / 1023;
}

void setup() {
  pinMode(WIPE, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
}

void loop() {
  if (digitalRead(WIPE) == LOW) {
    // A shake: the picture fades in three steps, as the aluminium powder settles.
    for (uint8_t k = 0; k < 3; k++) {
      for (uint16_t i = 0; i < 400; i++) display.drawPixel(random(128), random(64), SSD1306_BLACK);
      display.display();
    }
    display.clearDisplay();
    while (digitalRead(WIPE) == LOW) delay(10);
  }
  int16_t x = knob(KNOB_X, 128), y = 63 - knob(KNOB_Y, 64);
  if (penX >= 0) display.drawLine(penX, penY, x, y, SSD1306_WHITE);
  penX = x; penY = y;
  // The pen blinks so that it can be found.
  display.drawPixel(x, y, (millis() / 250) % 2 ? SSD1306_WHITE : SSD1306_INVERSE);
  display.display();
  delay(15);
}

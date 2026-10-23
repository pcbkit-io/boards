// Weather station: an SHT31 on I2C, a 128 x 64 SSD1306 OLED on I2C, a button on D2.
// Three pages, turned by the button: now (temperature, humidity, dew point, how it feels),
// the day's extremes, and a graph of the last two minutes. Change the weather in the lab and
// watch it follow; the same sketch builds in the Arduino IDE with the same libraries.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT31.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_SHT31 sht31;

const uint8_t PAGE_BUTTON = 2;
const uint8_t HISTORY = 60;          // samples on the graph, one every two seconds
int8_t temperatures[HISTORY];        // whole degrees
uint8_t humidities[HISTORY];
uint8_t samples, page;
float temperature, humidity, lowest = 99, highest = -99;
unsigned long sampledAt, pressedAt;

float dewPoint(float t, float rh) {
  // Magnus formula, over water.
  const float a = 17.62, b = 243.12;
  float g = log(rh / 100) + a * t / (b + t);
  return b * g / (a - g);
}

// Strings stay in flash (F()): the display's buffer takes half the Nano's 2 KB of RAM.
const __FlashStringHelper *feeling(float t, float rh) {
  if (t < 16) return F("cold");
  if (t > 27) return rh > 60 ? F("muggy") : F("hot");
  if (rh < 30) return F("dry");
  if (rh > 65) return F("damp");
  return F("comfortable");
}

void sample() {
  float t, h;
  if (!sht31.readBoth(&t, &h)) return;
  temperature = t;
  humidity = h;
  lowest = min(lowest, t);
  highest = max(highest, t);
  if (samples == HISTORY) {
    memmove(temperatures, temperatures + 1, HISTORY - 1);
    memmove(humidities, humidities + 1, HISTORY - 1);
    samples--;
  }
  temperatures[samples] = round(t);
  humidities[samples] = round(h);
  samples++;
}

void big(float value, uint8_t decimals, const __FlashStringHelper *unit, int16_t y) {
  display.setTextSize(3);
  display.setCursor(0, y);
  display.print(value, decimals);
  display.setTextSize(1);
  display.print(unit);
}

void now() {
  big(temperature, 1, F(" C"), 0);
  big(humidity, 0, F(" %RH"), 26);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("dew "));
  display.print(dewPoint(temperature, humidity), 1);
  display.print(F("  "));
  display.print(feeling(temperature, humidity));
}

void extremes() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("TODAY"));
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.print(F("lo "));
  display.print(lowest, 1);
  display.setCursor(0, 40);
  display.print(F("hi "));
  display.print(highest, 1);
}

void graph() {
  // Temperature as a line, humidity as dots, each scaled to its own range.
  int8_t tlo = 127, thi = -128;
  for (uint8_t i = 0; i < samples; i++) { tlo = min(tlo, temperatures[i]); thi = max(thi, temperatures[i]); }
  if (thi - tlo < 4) { tlo -= 2; thi += 2; }
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(thi);
  display.setCursor(0, 56);
  display.print(tlo);
  display.drawFastVLine(16, 0, 64, SSD1306_WHITE);
  for (uint8_t i = 1; i < samples; i++) {
    int16_t x0 = 18 + (i - 1) * 110 / (HISTORY - 1), x1 = 18 + i * 110 / (HISTORY - 1);
    int16_t y0 = 63 - (temperatures[i - 1] - tlo) * 63 / (thi - tlo), y1 = 63 - (temperatures[i] - tlo) * 63 / (thi - tlo);
    display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
    display.drawPixel(x1, 63 - humidities[i] * 63 / 100, SSD1306_WHITE);
  }
}

void setup() {
  pinMode(PAGE_BUTTON, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  sht31.begin(0x44);
  display.setTextColor(SSD1306_WHITE);
  sample();
}

void loop() {
  if (millis() - sampledAt >= 2000) {
    sampledAt = millis();
    sample();
  }
  if (digitalRead(PAGE_BUTTON) == LOW && millis() - pressedAt > 250) {
    pressedAt = millis();
    page = (page + 1) % 3;
  }
  display.clearDisplay();
  if (page == 0) now();
  else if (page == 1) extremes();
  else graph();
  display.display();
  delay(50);
}

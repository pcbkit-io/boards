// CO2 monitor: Sensirion SCD40 on I2C (its own Arduino library), a 128 x 64 SSD1306 OLED on I2C,
// a traffic light on D5-D7. A reading every five seconds; the graph keeps the last 120 (ten
// minutes). Raise the CO2 in the lab, as a room full of people does, and watch the light turn.
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SensirionI2cScd4x scd40;

const uint8_t GREEN = 5, AMBER = 6, RED = 7;
const uint8_t HISTORY = 120;
uint8_t history[HISTORY];           // ppm / 20, 0 … 255 (up to 5100 ppm)
uint8_t count;
uint16_t co2;
float temperature, humidity;
bool sensorFound;

void light() {
  bool blink = co2 > 2000 && (millis() / 400) % 2;
  digitalWrite(GREEN, co2 && co2 < 800);
  digitalWrite(AMBER, co2 >= 800 && co2 < 1400);
  digitalWrite(RED, co2 >= 1400 && !blink);
}

void draw() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  if (!sensorFound) {
    display.setCursor(0, 0);
    display.print(F("No SCD40 at 0x62"));
  } else if (!co2) {
    display.setCursor(0, 0);
    display.print(F("Warming up..."));
  } else {
    display.setTextSize(3);
    display.setCursor(0, 0);
    display.print(co2);
    display.setTextSize(1);
    display.print(F(" ppm"));
    display.setCursor(0, 26);
    display.print(temperature, 1);
    display.print(F(" C  "));
    display.print(humidity, 0);
    display.print(F(" %RH"));
    // The last ten minutes, 400 ppm at the bottom, 2000 at the top.
    for (uint8_t i = 0; i < count; i++) {
      int16_t ppm = history[i] * 20;
      int16_t h = constrain((ppm - 400) * 27L / 1600, 0, 27);
      display.drawFastVLine(i + 4, 63 - h, h + 1, SSD1306_WHITE);
    }
    display.drawFastHLine(0, 63 - (1000 - 400) * 27L / 1600, 4, SSD1306_WHITE);
  }
  display.display();
}

void setup() {
  pinMode(GREEN, OUTPUT);
  pinMode(AMBER, OUTPUT);
  pinMode(RED, OUTPUT);
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  scd40.begin(Wire, SCD40_I2C_ADDR_62);
  delay(30);
  scd40.wakeUp();
  scd40.stopPeriodicMeasurement();
  scd40.reinit();
  sensorFound = scd40.startPeriodicMeasurement() == 0;
}

void loop() {
  bool ready = false;
  if (sensorFound && scd40.getDataReadyStatus(ready) == 0 && ready && scd40.readMeasurement(co2, temperature, humidity) == 0) {
    if (count == HISTORY) { memmove(history, history + 1, HISTORY - 1); count--; }
    history[count++] = min(co2 / 20, 255);
  }
  light();
  draw();
  delay(100);
}

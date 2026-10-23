// Thermostat: SHT31 (I2C), SSD1306 OLED (I2C), a radiator valve on a servo (D9), − and + on D2 and
// D3, a heating light on D8. A PI controller turns the error into the valve's opening; its
// integral is clamped so that it does not wind up while the valve is fully open or shut.
#include <Wire.h>
#include <EEPROM.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT31.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_SHT31 sht31;
Servo valve;

const uint8_t MINUS = 2, PLUS = 3, HEAT = 8, VALVE = 9;
const float KP = 40, KI = 0.8;        // % opening per °C, and per °C·s
float target = 21.0, temperature = NAN, humidity, integral, opening;
unsigned long measuredAt, pressedAt;

void control(float dt) {
  float error = target - temperature;
  float output = KP * error + KI * integral;
  if ((output < 100 || error < 0) && (output > 0 || error > 0)) integral += error * dt;
  opening = constrain(KP * error + KI * integral, 0, 100);
  valve.write(map(opening, 0, 100, 0, 90));
  digitalWrite(HEAT, opening > 1);
}

void draw() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(3);
  display.setCursor(0, 0);
  if (isnan(temperature)) display.print(F("--.-")); else display.print(temperature, 1);
  display.setTextSize(1);
  display.print(F(" C"));
  display.setCursor(0, 28);
  display.print(F("target "));
  display.print(target, 1);
  display.print(F("  "));
  display.print(humidity, 0);
  display.print(F("%RH"));
  // The valve's opening as a bar.
  display.setCursor(0, 44);
  display.print(F("valve"));
  display.drawRect(34, 43, 90, 9, SSD1306_WHITE);
  display.fillRect(36, 45, opening * 86 / 100, 5, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print(opening > 1 ? F("heating") : F("idle"));
  display.display();
}

void setup() {
  pinMode(MINUS, INPUT_PULLUP);
  pinMode(PLUS, INPUT_PULLUP);
  pinMode(HEAT, OUTPUT);
  valve.attach(VALVE);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  sht31.begin(0x44);
  float saved;
  EEPROM.get(0, saved);
  if (saved >= 10 && saved <= 30) target = saved;
}

void loop() {
  if (millis() - pressedAt > 200) {
    float before = target;
    if (digitalRead(MINUS) == LOW) target -= 0.5;
    if (digitalRead(PLUS) == LOW) target += 0.5;
    target = constrain(target, 10, 30);
    if (target != before) { pressedAt = millis(); EEPROM.put(0, target); }
  }
  if (millis() - measuredAt >= 1000) {
    float dt = (millis() - measuredAt) / 1000.0;
    measuredAt = millis();
    if (sht31.readBoth(&temperature, &humidity)) control(dt);
  }
  draw();
  delay(20);
}

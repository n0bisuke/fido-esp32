#include <Arduino.h>

// Absolute minimal test: just blink and Serial
// No TinyUSB, no NeoPixel, nothing fancy

void setup() {
  pinMode(21, OUTPUT);  // LED_BUILTIN on XIAO ESP32-S3
  Serial.begin(115200);
}

void loop() {
  digitalWrite(21, !digitalRead(21));
  Serial.println("alive");
  delay(1000);
}
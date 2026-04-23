#include <Arduino.h>

// Minimal TinyUSB CDC test: use Serial only (no extra CDC instance)
// ARDUINO_USB_CDC_ON_BOOT=1 means framework calls Serial.begin() for us

void setup() {
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  // Serial is already initialized by framework (ARDUINO_USB_CDC_ON_BOOT=1)
}

void loop() {
  Serial.println("tinyusb alive");
  digitalWrite(21, !digitalRead(21));
  delay(1000);
}
#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "hid_descriptor.h"
#include "ctap2.h"
#include "crypto_wrapper.h"
#include "key_storage.h"

// --- GPIO21 = built-in LED on XIAO ESP32-S3 ---
#define LED_PIN 21

// --- USB HID Instance (FIDO2) ---
Adafruit_USBD_HID usb_hid(hid_report_descriptor, hid_report_descriptor_len,
                           HID_ITF_PROTOCOL_NONE, 2, true);

// --- Default: debug mode. BOOT button held during first 2 seconds → HID mode ---
bool debug_mode = true;

// --- HID Callbacks ---
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type,
                             uint8_t *buffer, uint16_t reqlen) {
  (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
  return 0;
}

void set_report_callback(uint8_t report_id, hid_report_type_t report_type,
                         uint8_t const *buffer, uint16_t bufsize) {
  (void)report_id; (void)report_type;
  if (debug_mode) {
    Serial.printf("[HID] set_report: %u bytes\n", bufsize);
  }
  ctap2_process_hid_report(buffer, bufsize);
}

// --- Setup ---
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED on during init

  // BOOT button (GPIO0, active LOW) - check FIRST before any init
  pinMode(0, INPUT_PULLUP);
  delay(50); // settle time for pull-up
  debug_mode = (digitalRead(0) == HIGH); // not pressed = debug mode
  // BOOT pressed during boot = HID mode (debug_mode = false)

  // Serial is already initialized by framework (ARDUINO_USB_CDC_ON_BOOT=1)
  ctap2_init();
  crypto_init();
  key_storage_init();

  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();

  if (debug_mode) {
    // Debug mode: LED will blink in loop()
    Serial.println("=== DEBUG MODE ===");
    Serial.println("FIDO2 v0.3 (XIAO ESP32-S3)");
    digitalWrite(LED_PIN, LOW);
  } else {
    // HID mode: LED solid on
    digitalWrite(LED_PIN, HIGH);
  }
}

// --- Loop ---
void loop() {
  ctap2_process_pending();

  // Debug mode: LED blink
  if (debug_mode) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(200);
  }
}
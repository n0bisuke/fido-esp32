#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

#include "hid_descriptor.h"
#include "ctap2.h"
#include "crypto_wrapper.h"
#include "key_storage.h"

// --- USB HID Instance (FIDO2) ---
Adafruit_USBD_HID usb_hid(hid_report_descriptor, hid_report_descriptor_len,
                           HID_ITF_PROTOCOL_NONE, 2, true);

// --- Debug mode: BOOT button held during first 2 seconds → enable Serial output ---
bool debug_mode = false;

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
  // GPIO21 = built-in LED on XIAO ESP32-S3
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  // BOOT button (GPIO0, active LOW)
  pinMode(0, INPUT_PULLUP);

  // Serial is already initialized by framework (ARDUINO_USB_CDC_ON_BOOT=1)
  ctap2_init();
  crypto_init();
  key_storage_init();

  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();

  // Wait for USB enumeration and check BOOT button for debug mode
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (digitalRead(0) == LOW) {
      debug_mode = true;
    }
    delay(50);
  }

  if (debug_mode) {
    Serial.println("=== DEBUG MODE ===");
    Serial.println("FIDO2 v0.3 (XIAO ESP32-S3)");
    Serial.println("BOOT button detected: Serial output enabled");
    digitalWrite(21, LOW); // LED off = debug mode
  } else {
    digitalWrite(21, LOW); // LED off after init
  }
}

// --- Loop ---
void loop() {
  ctap2_process_pending();
}
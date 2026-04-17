#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <M5GFX.h>

#include "hid_descriptor.h"

// --- USB HID Instance ---
// has_out_endpoint=true: CTAP2 uses OUT endpoint for host->device
Adafruit_USBD_HID usb_hid(hid_report_descriptor, hid_report_descriptor_len,
                           HID_ITF_PROTOCOL_NONE, 2, true);

// --- Display ---
M5GFX display;

// --- HID Callbacks ---
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type,
                             uint8_t *buffer, uint16_t reqlen) {
  // TODO: respond to GET_REPORT from host
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}

void set_report_callback(uint8_t report_id, hid_report_type_t report_type,
                         uint8_t const *buffer, uint16_t bufsize) {
  // TODO: parse incoming CTAP2 request from host
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  // USB HID init
  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();

  // Display init
  display.init();
#if defined(ATOMS3)
  // AtomS3: 0.96" 128x128, button-friendly rotation
  display.setRotation(1);
  display.setTextSize(2);
#elif defined(M5STICKC_S3)
  // M5StickC S3: 1.14" 135x240
  display.setRotation(1);
  display.setTextSize(2);
#endif
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setCursor(0, 0);
  display.print("READY");

  Serial.println("[esp-fido] initialized");
}

// --- Loop ---
void loop() {
  // TODO: pattern gate state machine
  // TODO: CTAP2 command processing
}
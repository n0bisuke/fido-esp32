#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <M5GFX.h>

#include "hid_descriptor.h"
#include "ctap2.h"
#include "crypto_wrapper.h"
#include "key_storage.h"

// --- USB HID Instance ---
Adafruit_USBD_HID usb_hid(hid_report_descriptor, hid_report_descriptor_len,
                           HID_ITF_PROTOCOL_NONE, 2, true);

// --- Display ---
M5GFX display;

// --- Debug display ---
static void lcd_print(const char *msg, int line = 0) {
  display.fillRect(0, line * 16, 128, 16, TFT_BLACK);
  display.setCursor(0, line * 16);
  display.setTextSize(1);
  display.print(msg);
}

// --- HID Callbacks ---
uint16_t get_report_callback(uint8_t report_id, hid_report_type_t report_type,
                             uint8_t *buffer, uint16_t reqlen) {
  (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
  return 0;
}

void set_report_callback(uint8_t report_id, hid_report_type_t report_type,
                         uint8_t const *buffer, uint16_t bufsize) {
  (void)report_id; (void)report_type;
  // Direct LCD debug: is HID callback even called?
  display.fillRect(0, 32, 128, 16, TFT_BLACK);
  display.setCursor(0, 32);
  display.setTextSize(1);
  display.printf("HID %u", bufsize);
  ctap2_process_hid_report(buffer, bufsize);
}

// --- Setup ---
void setup() {
  Serial.begin(115200);

  ctap2_init();
  crypto_init();
  key_storage_init();

  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();

  display.init();
#if defined(ATOMS3)
  display.setRotation(1);
  display.setTextSize(1);
#elif defined(M5STICKC_S3)
  display.setRotation(1);
  display.setTextSize(1);
#endif
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE);
  display.setCursor(0, 0);
  display.print("FIDO2 v0.3");
}

// --- Loop ---
void loop() {
  ctap2_process_pending();
}
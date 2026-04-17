#ifndef HID_DESCRIPTOR_H
#define HID_DESCRIPTOR_H

#include <stdint.h>

// FIDO Alliance HID Usage Page
#define FIDO_USAGE_PAGE    0xF1D0
#define FIDO_USAGE         0x01

// CTAP2 HID report size
#define CTAP2_REPORT_SIZE  64

// USB HID Report Descriptor for FIDO2 Authenticator
static const uint8_t hid_report_descriptor[] = {
    0x06, 0xD0, 0xF1,       // Usage Page (FIDO Alliance) 0xF1D0
    0x09, 0x01,              // Usage (FIDO Authenticator) 0x01
    0xA1, 0x01,              // Collection (Application)
    0x09, 0x20,              //   Usage (Data In)
    0x15, 0x00,              //   Logical Minimum (0)
    0x26, 0xFF, 0x00,        //   Logical Maximum (255)
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x40,              //   Report Count (64)
    0x81, 0x02,              //   Input (Data, Variable, Absolute)
    0x09, 0x21,              //   Usage (Data Out)
    0x15, 0x00,              //   Logical Minimum (0)
    0x26, 0xFF, 0x00,        //   Logical Maximum (255)
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x40,              //   Report Count (64)
    0x91, 0x02,              //   Output (Data, Variable, Absolute)
    0xC0                     // End Collection
};

static const uint16_t hid_report_descriptor_len = sizeof(hid_report_descriptor);

#endif // HID_DESCRIPTOR_H
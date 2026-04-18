#ifndef CTAP2_H
#define CTAP2_H

#include <stdint.h>
#include <stddef.h>

// CTAPHID command codes (bit 7 set for INIT packet)
#define CTAPHID_MSG        0x83
#define CTAPHID_INIT       0x86
#define CTAPHID_PING       0x80
#define CTAPHID_ERROR      0xBF
#define CTAPHID_KEEPALIVE  0xBB

// CTAP2 command codes (inside CTAPHID_MSG payload)
#define CTAP2_MAKE_CREDENTIAL  0x01
#define CTAP2_GET_ASSERTION    0x02
#define CTAP2_GET_INFO         0x04
#define CTAP2_CLIENT_PIN       0x06
#define CTAP2_RESET            0x07
#define CTAP2_SELECTION        0x0B

// CTAP2 status codes
#define CTAP2_OK                    0x00
#define CTAP2_ERR_INVALID_COMMAND   0x01
#define CTAP2_ERR_INVALID_PARAM     0x02
#define CTAP2_ERR_INVALID_LENGTH    0x03
#define CTAP2_ERR_INVALID_SEQ       0x04
#define CTAP2_ERR_TIMEOUT           0x05
#define CTAP2_ERR_ACTION_TIMEOUT    0x2E
#define CTAP2_ERR_UNSUPPORTED_ALGORITHM 0x26

// CTAPHID error codes (sent via CTAPHID_ERROR)
#define CTAPHID_ERR_INVALID_CMD  0x01
#define CTAPHID_ERR_INVALID_PAR  0x02
#define CTAPHID_ERR_INVALID_LEN  0x03
#define CTAPHID_ERR_INVALID_SEQ  0x04
#define CTAPHID_ERR_MSG_TIMEOUT  0x05
#define CTAPHID_ERR_CHANNEL_BUSY 0x06
#define CTAPHID_ERR_LOCK_REQUIRED 0x0A
#define CTAPHID_ERR_INVALID_CID  0x0B

// CTAPHID protocol constants
#define CTAPHID_BROADCAST_CID   0xFFFFFFFF
#define CTAPHID_INIT_NONCE_SIZE 8
#define CTAPHID_MAX_MSG_SIZE    1200
#define CTAPHID_INIT_PAYLOAD_SIZE 17

// HID report size
#define CTAPHID_PACKET_SIZE    64
#define CTAPHID_INIT_HEADER_SIZE 7
#define CTAPHID_CONT_HEADER_SIZE 5
#define CTAPHID_INIT_MAX_DATA  (CTAPHID_PACKET_SIZE - CTAPHID_INIT_HEADER_SIZE) // 57
#define CTAPHID_CONT_MAX_DATA  (CTAPHID_PACKET_SIZE - CTAPHID_CONT_HEADER_SIZE) // 59

void ctap2_init();
void ctap2_process_hid_report(const uint8_t *buf, uint16_t len);
void ctap2_process_pending();

#endif // CTAP2_H
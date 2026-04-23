#include "ctap2.h"
#include "authenticator.h"
#include <Adafruit_TinyUSB.h>
#include <USB.h>
#include <Arduino.h>
#include <string.h>

// External USB instances (defined in main.cpp)
extern Adafruit_USBD_HID usb_hid;
extern bool debug_mode;

// --- TX state (outgoing multi-packet response) ---
static struct {
  uint8_t buf[CTAPHID_MAX_MSG_SIZE];
  uint16_t total_len;
  uint16_t sent;
  uint8_t seq;
  uint32_t cid;
  bool active;
} tx_state;

// --- RX state (incoming multi-packet reassembly) ---
static struct {
  uint8_t buf[CTAPHID_MAX_MSG_SIZE];
  uint16_t total_len;
  uint16_t received;
  uint8_t seq;
  uint32_t cid;
  uint8_t cmd; // from INIT packet header
  bool active;
} rx_state;

// --- Channel ID allocation ---
static uint32_t next_cid = 1;

// --- Pending request (deferred to main loop for sufficient stack) ---
static struct {
  uint8_t data[CTAPHID_MAX_MSG_SIZE];
  uint16_t len;
  uint32_t cid;
  bool pending;
} pending_req;

static uint32_t allocate_cid() {
  uint32_t cid = next_cid++;
  if (cid == CTAPHID_BROADCAST_CID) cid = next_cid++;
  return cid;
}

// --- Send a single 64-byte HID report ---
static void send_hid_report(const uint8_t *pkt) {
  usb_hid.sendReport(0, pkt, CTAPHID_PACKET_SIZE);
}

// --- Build and send CTAPHID error ---
static void ctap2_send_error(uint32_t cid, uint8_t error_code) {
  uint8_t pkt[CTAPHID_PACKET_SIZE] = {0};
  pkt[0] = (cid >> 24) & 0xFF;
  pkt[1] = (cid >> 16) & 0xFF;
  pkt[2] = (cid >> 8) & 0xFF;
  pkt[3] = cid & 0xFF;
  pkt[4] = CTAPHID_ERROR;
  pkt[5] = 0;
  pkt[6] = 1;
  pkt[7] = error_code;
  send_hid_report(pkt);
}

// --- Send CTAPHID response (multi-packet aware) ---
static void ctap2_send_response(uint32_t cid, uint8_t cmd, const uint8_t *data, uint16_t len) {
  uint8_t pkt[CTAPHID_PACKET_SIZE] = {0};
  pkt[0] = (cid >> 24) & 0xFF;
  pkt[1] = (cid >> 16) & 0xFF;
  pkt[2] = (cid >> 8) & 0xFF;
  pkt[3] = cid & 0xFF;
  pkt[4] = cmd;
  pkt[5] = (len >> 8) & 0xFF;
  pkt[6] = len & 0xFF;

  uint16_t copy_len = (len > CTAPHID_INIT_MAX_DATA) ? CTAPHID_INIT_MAX_DATA : len;
  memcpy(&pkt[CTAPHID_INIT_HEADER_SIZE], data, copy_len);
  send_hid_report(pkt);

  if (len > CTAPHID_INIT_MAX_DATA) {
    tx_state.cid = cid;
    tx_state.total_len = len;
    tx_state.sent = copy_len;
    tx_state.seq = 0;
    tx_state.active = true;
    memcpy(tx_state.buf, data, len);
  }
}

// --- Continuation packet sender (called when IN endpoint is free) ---
extern "C" void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
  (void)instance; (void)report; (void)len;
  if (!tx_state.active) return;
  if (tx_state.sent >= tx_state.total_len) {
    tx_state.active = false;
    return;
  }

  uint8_t pkt[CTAPHID_PACKET_SIZE] = {0};
  pkt[0] = (tx_state.cid >> 24) & 0xFF;
  pkt[1] = (tx_state.cid >> 16) & 0xFF;
  pkt[2] = (tx_state.cid >> 8) & 0xFF;
  pkt[3] = tx_state.cid & 0xFF;
  pkt[4] = tx_state.seq;

  uint16_t remaining = tx_state.total_len - tx_state.sent;
  uint16_t copy_len = (remaining > CTAPHID_CONT_MAX_DATA) ? CTAPHID_CONT_MAX_DATA : remaining;
  memcpy(&pkt[CTAPHID_CONT_HEADER_SIZE], &tx_state.buf[tx_state.sent], copy_len);
  tx_state.sent += copy_len;
  tx_state.seq++;

  send_hid_report(pkt);

  if (tx_state.sent >= tx_state.total_len) {
    tx_state.active = false;
  }
}

// --- Handle CTAPHID_INIT ---
static void ctap2_handle_init(uint32_t cid, const uint8_t *data, uint16_t len) {
  if (len < CTAPHID_INIT_NONCE_SIZE) return;

  uint8_t resp[CTAPHID_INIT_PAYLOAD_SIZE];
  // Echo nonce
  memcpy(resp, data, CTAPHID_INIT_NONCE_SIZE);
  // Allocated CID
  uint32_t new_cid = allocate_cid();
  resp[8]  = (new_cid >> 24) & 0xFF;
  resp[9]  = (new_cid >> 16) & 0xFF;
  resp[10] = (new_cid >> 8) & 0xFF;
  resp[11] = new_cid & 0xFF;
  // Protocol version
  resp[12] = 2; // CTAP2
  // Major/Minor/Build version
  resp[13] = 1;
  resp[14] = 0;
  resp[15] = 0;
  // Capabilities: 0x04 = CBOR (wink not supported)
  resp[16] = 0x04;

  ctap2_send_response(CTAPHID_BROADCAST_CID, CTAPHID_INIT, resp, CTAPHID_INIT_PAYLOAD_SIZE);
}

// --- Handle CTAPHID_MSG (CTAP2 commands) ---
static void ctap2_handle_msg(uint32_t cid, const uint8_t *data, uint16_t len) {
  if (len < 1) {
    ctap2_send_error(cid, CTAPHID_ERR_INVALID_LEN);
    return;
  }

  uint8_t ctap_cmd = data[0];
  uint8_t resp[CTAPHID_MAX_MSG_SIZE];
  size_t resp_len = 0;

  switch (ctap_cmd) {
  case CTAP2_GET_INFO:
    authenticator_get_info(resp, &resp_len);
    ctap2_send_response(cid, CTAPHID_MSG, resp, resp_len);
    break;
  case CTAP2_MAKE_CREDENTIAL:
  case CTAP2_GET_ASSERTION:
    // Defer heavy crypto to main loop (needs more stack than HID callback context)
    if (pending_req.pending) {
      resp[0] = CTAP2_ERR_ACTION_TIMEOUT;
      resp_len = 1;
      ctap2_send_response(cid, CTAPHID_MSG, resp, resp_len);
    } else {
      memcpy(pending_req.data, data, len);
      pending_req.len = len;
      pending_req.cid = cid;
      pending_req.pending = true;
      if (debug_mode) Serial.printf("[CTAP2] pending: %s\n", ctap_cmd == CTAP2_MAKE_CREDENTIAL ? "MakeCredential" : "GetAssertion");
    }
    break;
  case CTAP2_SELECTION:
    authenticator_selection(resp, &resp_len);
    ctap2_send_response(cid, CTAPHID_MSG, resp, resp_len);
    break;
  default:
    resp[0] = CTAP2_ERR_INVALID_COMMAND;
    resp_len = 1;
    ctap2_send_response(cid, CTAPHID_MSG, resp, resp_len);
    break;
  }
}

// --- Dispatch fully reassembled CTAPHID command ---
static void ctap2_dispatch(uint32_t cid, uint8_t cmd, const uint8_t *data, uint16_t len) {
  const char *name = cmd == CTAPHID_INIT ? "INIT" : cmd == CTAPHID_MSG ? "MSG" : "PING";
  if (debug_mode) Serial.printf("[CTAPHID] %s cid=0x%06X len=%u\n", name, cid, len);
  switch (cmd) {
  case CTAPHID_INIT:
    ctap2_handle_init(cid, data, len);
    break;
  case CTAPHID_MSG:
    ctap2_handle_msg(cid, data, len);
    break;
  case CTAPHID_PING:
    ctap2_send_response(cid, CTAPHID_PING, data, len);
    break;
  default:
    ctap2_send_error(cid, CTAPHID_ERR_INVALID_CMD);
    break;
  }
}

// --- Process incoming HID report (called from set_report_callback) ---
void ctap2_process_hid_report(const uint8_t *buf, uint16_t len) {
  if (len < CTAPHID_INIT_HEADER_SIZE) return;

  uint32_t cid = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                 ((uint32_t)buf[2] << 8) | buf[3];
  uint8_t cmd_or_seq = buf[4];

  if (cmd_or_seq & 0x80) {
    // INIT packet
    uint8_t cmd = cmd_or_seq;
    uint16_t data_len = ((uint16_t)buf[5] << 8) | buf[6];

    if (rx_state.active && rx_state.cid != cid) {
      ctap2_send_error(cid, CTAPHID_ERR_CHANNEL_BUSY);
      return;
    }

    rx_state.cid = cid;
    rx_state.total_len = data_len;
    rx_state.received = 0;
    rx_state.seq = 0;
    rx_state.cmd = cmd;
    rx_state.active = true;

    uint16_t copy_len = (data_len > CTAPHID_INIT_MAX_DATA) ? CTAPHID_INIT_MAX_DATA : data_len;
    memcpy(rx_state.buf, &buf[CTAPHID_INIT_HEADER_SIZE], copy_len);
    rx_state.received = copy_len;

    if (rx_state.received >= rx_state.total_len) {
      rx_state.active = false;
      ctap2_dispatch(cid, cmd, rx_state.buf, rx_state.total_len);
    }
  } else if (rx_state.active && cid == rx_state.cid) {
    // CONT packet
    uint8_t seq = cmd_or_seq;
    if (seq != rx_state.seq) {
      ctap2_send_error(cid, CTAPHID_ERR_INVALID_SEQ);
      rx_state.active = false;
      return;
    }
    uint16_t remaining = rx_state.total_len - rx_state.received;
    uint16_t copy_len = (remaining > CTAPHID_CONT_MAX_DATA) ? CTAPHID_CONT_MAX_DATA : remaining;
    memcpy(&rx_state.buf[rx_state.received], &buf[CTAPHID_CONT_HEADER_SIZE], copy_len);
    rx_state.received += copy_len;
    rx_state.seq++;

    if (rx_state.received >= rx_state.total_len) {
      rx_state.active = false;
      ctap2_dispatch(rx_state.cid, rx_state.cmd, rx_state.buf, rx_state.total_len);
    }
  }
}

// --- Init ---
void ctap2_init() {
  memset(&tx_state, 0, sizeof(tx_state));
  memset(&rx_state, 0, sizeof(rx_state));
  memset(&pending_req, 0, sizeof(pending_req));
}

// --- Process pending MakeCredential/GetAssertion (called from main loop) ---
void ctap2_process_pending() {
  if (!pending_req.pending) return;
  pending_req.pending = false;

  uint8_t resp[CTAPHID_MAX_MSG_SIZE];
  size_t resp_len = 0;

  switch (pending_req.data[0]) {
  case CTAP2_MAKE_CREDENTIAL:
    authenticator_make_credential(pending_req.data, pending_req.len, resp, &resp_len);
    break;
  case CTAP2_GET_ASSERTION:
    authenticator_get_assertion(pending_req.data, pending_req.len, resp, &resp_len);
    break;
  default:
    resp[0] = CTAP2_ERR_INVALID_COMMAND;
    resp_len = 1;
    break;
  }

  if (debug_mode) Serial.printf("[CTAP2] response: status=0x%02x len=%u\n", resp[0], resp_len);
  ctap2_send_response(pending_req.cid, CTAPHID_MSG, resp, resp_len);
}
# M5Stack AtomS3で自作FIDO2セキュリティキーを作る ― ブラウザに認識させるまで

## はじめに

Googleアカウントの二段階認証に使うYubiKeyのようなセキュリティキーを、M5Stack AtomS3（ESP32-S3）で自作しました。この記事では、ブラウザに「セキュリティキーを検出」と表示させるまでの最小実装を紹介します。

**対象読者**: ESP32でUSBデバイスを作ってみたい人、FIDO2/CTAP2の中身を知りたい人

**前提環境**:
- M5Stack AtomS3 (ESP32-S3, 8MB Flash)
- PlatformIO (Arduino framework)
- Adafruit TinyUSB
- macOS (ホストマシン)

## FIDO2/CTAP2の超概要

FIDO2セキュリティキーは、USB HIDデバイスとしてPCに接続します。ブラウザとはCTAP2プロトコルで通信し、メッセージはCBORというバイナリ形式でエンコードされます。

```
ブラウザ (WebAuthn API)
    ↓ CTAP2リクエスト (CBOR) をUSB HIDで送信
セキュリティキー (ESP32-S3)
    ↓ CBORパース → 処理 → CBORレスポンス生成
    ↓ USB HIDでレスポンスを返す
ブラウザ
    ↓ 署名を検証
ログイン成功
```

## 実装の全体像

最小構成は以下の4ファイルです:

```
src/
├── main.cpp           # エントリポイント、USB HID初期化
├── hid_descriptor.h   # FIDO2 HIDレポートディスクリプタ
├── ctap2.h/cpp        # CTAPHIDフレーミング・コマンドディスパッチ
└── authenticator.h/cpp # authenticatorGetInfoのCBOR応答生成
```

## 1. USB HIDディスクリプタ

FIDO2セキュリティキーとして認識されるには、Usage Page `0xF1D0` を指定したHIDディスクリプタが必要です。

```cpp
// hid_descriptor.h
static const uint8_t hid_report_descriptor[] = {
    0x06, 0xD0, 0xF1,       // Usage Page (FIDO Alliance) 0xF1D0
    0x09, 0x01,              // Usage (FIDO Authenticator)
    0xA1, 0x01,              // Collection (Application)
    0x09, 0x20,              //   Usage (Data In)
    0x15, 0x00,              //   Logical Minimum (0)
    0x26, 0xFF, 0x00,        //   Logical Maximum (255)
    0x75, 0x08,              //   Report Size (8)
    0x95, 0x40,              //   Report Count (64)
    0x81, 0x02,              //   Input (Data, Variable, Absolute)
    0x09, 0x21,              //   Usage (Data Out)
    0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, 0x40,
    0x91, 0x02,              //   Output (Data, Variable, Absolute)
    0xC0                     // End Collection
};
```

**ポイント**: レポートサイズは64バイト。CTAP2の通信はこの64バイト単位で行われます。

## 2. USB HID初期化 (main.cpp)

ESP32-S3の内蔵USB-OTGとAdafruit TinyUSBを使います。OUTエンドポイントを有効にするのが重要です（ホストからのデータ受信に必要）。

```cpp
#include <Adafruit_TinyUSB.h>
#include "hid_descriptor.h"
#include "ctap2.h"

// OUTエンドポイント有効 = true（CTAP2はホスト→デバイス通信にOUT EPを使う）
Adafruit_USBD_HID usb_hid(hid_report_descriptor, hid_report_descriptor_len,
                           HID_ITF_PROTOCOL_NONE, 2, true);

void set_report_callback(uint8_t report_id, hid_report_type_t report_type,
                         uint8_t const *buffer, uint16_t bufsize) {
  ctap2_process_hid_report(buffer, bufsize);
}

void setup() {
  ctap2_init();
  usb_hid.setReportCallback(get_report_callback, set_report_callback);
  usb_hid.begin();
  // LCD初期化等...
}
```

**ハマりどころ**: `setReportCallback`は2引数（get_report, set_report）が必要です。1引数版はコンパイルエラーになります。

## 3. CTAPHIDフレーミング (ctap2.cpp)

CTAP2メッセージはCTAPHIDというフレーミング層で64バイトHIDレポートに分割されます。

### パケット構造

**INITパケット**（コマンドの先頭）:
```
[0-3] CID (4B)
[4]   CMD (0x80ビット付き = INIT)
[5-6] ペイロード長 (2B, ビッグエンディアン)
[7-63] データ (最大57B)
```

**CONTパケット**（継続データ）:
```
[0-3] CID (4B)
[4]   SEQ (0x00, 0x01... = 0x80ビットなし)
[5-63] データ (最大59B)
```

### チャンネル割り当て (CTAPHID_INIT)

ブラウザは最初にINITコマンドを送ってチャンネルIDを要求します:

```cpp
static void ctap2_handle_init(uint32_t cid, const uint8_t *data, uint16_t len) {
  uint8_t resp[17];
  memcpy(resp, data, 8);              // 8B nonceをエコー
  uint32_t new_cid = next_cid++;       // 新しいCIDを割り当て
  resp[8]  = (new_cid >> 24) & 0xFF;
  resp[9]  = (new_cid >> 16) & 0xFF;
  resp[10] = (new_cid >> 8) & 0xFF;
  resp[11] = new_cid & 0xFF;
  resp[12] = 2;    // CTAP2 protocol version
  resp[13] = 1;    // Major
  resp[14] = 0;    // Minor
  resp[15] = 0;    // Build
  resp[16] = 0x04; // Capabilities: CBOR support
  ctap2_send_response(CTAPHID_BROADCAST_CID, CTAPHID_INIT, resp, 17);
}
```

### マルチパケット送信

GetInfoのレスポンスは約74バイトで、1パケット（57バイト）に収まりません。INITパケット→CONTパケットの2回に分けて送信します:

```cpp
static void ctap2_send_response(uint32_t cid, uint8_t cmd,
                                const uint8_t *data, uint16_t len) {
  // INITパケットを構築して送信
  uint8_t pkt[64] = {0};
  // ... CID + CMD + LEN + 最初の57バイト ...
  usb_hid.sendReport(0, pkt, 64);

  if (len > 57) {
    // 残りは tud_hid_report_complete_cb で送信
    tx_state.active = true;
    // ...
  }
}

// INエンドポイントの送信完了コールバック
extern "C" void tud_hid_report_complete_cb(uint8_t instance, ...) {
  if (!tx_state.active) return;
  // CONTパケットを構築して送信
  uint8_t pkt[64] = {0};
  // ... CID + SEQ + 残りデータ ...
  usb_hid.sendReport(0, pkt, 64);
}
```

**ポイント**: `sendReport`は前の送信が完了してから呼ぶ必要があります。`tud_hid_report_complete_cb`で送信完了を検知してから次のパケットを送ります。

## 4. authenticatorGetInfoの実装

ここがこの記事のハイライトです。ブラウザがセキュリティキーを認識するための最小コマンドです。

### GetInfoレスポンスのCBOR構造

```
{
  0x01: ["FIDO_2_0", "U2F_V2"],     // サポートバージョン
  0x02: [],                          // エクステンション (なし)
  0x03: h'0000...00',               // AAGUID (16B, ゼロ = self-attestation)
  0x04: {                           // オプション
    "rk": false,                     // resident key: 非対応
    "up": true,                      // user presence: 対応
    "uv": true,                      // user verification: 対応
    "plat": false,                   // platform authenticator: なし
    "clientPin": false               // ClientPIN: 非対応
  },
  0x05: 1200                        // 最大メッセージサイズ
}
```

### tinycborでエンコード

```cpp
#include <cbor.h>

uint8_t authenticator_get_info(uint8_t *buf, size_t *out_len) {
  buf[0] = 0x00; // CTAP2_OK
  CborEncoder enc, map_enc, arr_enc, opt_enc;
  cbor_encoder_init(&enc, &buf[1], 1200 - 1, 0);
  cbor_encoder_create_map(&enc, &map_enc, 5);

  // versions
  cbor_encode_uint(&map_enc, 0x01);
  cbor_encoder_create_array(&map_enc, &arr_enc, 2);
  cbor_encode_text_stringz(&arr_enc, "FIDO_2_0");
  cbor_encode_text_stringz(&arr_enc, "U2F_V2");
  cbor_encoder_close_container(&map_enc, &arr_enc);

  // extensions
  cbor_encode_uint(&map_enc, 0x02);
  cbor_encoder_create_array(&map_enc, &arr_enc, 0);
  cbor_encoder_close_container(&map_enc, &arr_enc);

  // aaguid (16 zero bytes = self-attestation)
  cbor_encode_uint(&map_enc, 0x03);
  cbor_encode_byte_string(&map_enc, AAGUID, 16);

  // options
  cbor_encode_uint(&map_enc, 0x04);
  cbor_encoder_create_map(&map_enc, &opt_enc, 5);
  cbor_encode_text_stringz(&opt_enc, "rk");        cbor_encode_boolean(&opt_enc, false);
  cbor_encode_text_stringz(&opt_enc, "up");        cbor_encode_boolean(&opt_enc, true);
  cbor_encode_text_stringz(&opt_enc, "uv");        cbor_encode_boolean(&opt_enc, true);
  cbor_encode_text_stringz(&opt_enc, "plat");      cbor_encode_boolean(&opt_enc, false);
  cbor_encode_text_stringz(&opt_enc, "clientPin"); cbor_encode_boolean(&opt_enc, false);
  cbor_encoder_close_container(&map_enc, &opt_enc);

  // maxMsgSize
  cbor_encode_uint(&map_enc, 0x05);
  cbor_encode_uint(&map_enc, 1200);

  cbor_encoder_close_container(&enc, &map_enc);
  *out_len = 1 + cbor_encoder_get_buffer_size(&enc, &buf[1]);
  return 0x00;
}
```

## 5. 動かしてみる

```bash
# PlatformIOでビルド＆フラッシュ
pio run -e atoms3 -t upload
```

Chromeで https://webauthn.io にアクセスし、「Register」ボタンをクリックすると、ブラウザがセキュリティキーを検索します。AtomS3が認識されれば成功です！

（※MakeCredentialが未実装なので登録自体は失敗しますが、デバイスが検出されることでGetInfoの動作が確認できます）

## 今後の実装予定

- [ ] authenticatorMakeCredential（クレデンシャル登録）
- [ ] authenticatorGetAssertion（ログイン認証）
- [ ] ECDSA署名（micro-ecc使用）
- [ ] パターン認証ゲート（ボタン4連打で承認）

## つまずきポイントまとめ

| 問題 | 解決策 |
|------|--------|
| ESP32-S3のTinyUSB設定がsdkconfig形式 | `CONFIG_TINYUSB_*` を `sdkconfig.defaults` に書く（`CFG_TUD_*`ではない） |
| `setReportCallback`が1引数でエラー | 2引数（get_report, set_report）必須 |
| M5StickC S3がPlatformIOにボード定義がない | `boards/m5stickc-s3.json` を自作 |
| GetInfoレスポンスが57Bを超える | INIT+CONTの2パケット分割＋`tud_hid_report_complete_cb`で継続送信 |
| ホストマシンでpioコマンドが見つからない | `~/.zshrc` の古いaliasを削除、Homebrew版を使う |

## リポジトリ

https://github.com/your-username/esp-fido （※公開時はURLを更新）
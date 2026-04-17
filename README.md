# ESP-FIDO: M5StickC S3 FIDO2 Security Key

M5StickC S3 (ESP32-S3) を用いた自作FIDO2/CTAP2セキュリティキー。  
Googleアカウント等のWebAuthnログインに使用可能。

通常の「ボタン1回押し」によるユーザー承認を、**パターン認証ゲート**に置き換えている点が特徴。

## Hardware

| Item | Spec |
|------|------|
| MCU | ESP32-S3 (Dual-core 240MHz, 512KB SRAM) |
| Board | M5StickC S3 |
| USB | USB-C (ESP32-S3 内蔵 USB-OTG) |
| Display | 1.14" IPS LCD (135x240, ST7789) |
| Buttons | BtnA (GPIO37: 内蔵), BtnB (GPIO35: 側面, オプション) |
| Flash | 8MB (PSRAM 2MB) |

## Framework & Build

- **Framework**: Arduino (PlatformIO)
- **USB Stack**: Adafruit TinyUSB (ESP32-S3 内蔵USB-OTG対応)
- **Build System**: PlatformIO

```ini
; platformio.ini (予定)
[env:m5stickc-s3]
platform = espressif32
board = m5stickc-s3
framework = arduino
lib_deps =
    adafruit/Adafruit TinyUSB Library @ ^2.3
    m5stack/M5StickC-S3 @ ^0.1.0
    micro-ecc/micro-ecc @ ^1.0
    qubole/ArduinoCBOR @ ^0.5
```

## FIDO2/CTAP2 Specification

### USB HID Descriptor

```
Usage Page:    0xF1D0  (FIDO Alliance HID)
Usage:         0x01   (Authenticator)
Collection:     Application
  Report ID:   0x01 (not used; single report)
  Report Size:  64 bytes (input/output/feature)
```

| Endpoint | Direction | Size | Purpose |
|----------|-----------|------|---------|
| EP1 IN   | Device→Host | 64B | CTAP2 response |
| EP1 OUT  | Host→Device | 64B | CTAP2 request |

### CTAP2 Commands

| Command | Code | Status |
|---------|------|--------|
| `authenticatorMakeCredential` | 0x01 | Implement |
| `authenticatorGetAssertion` | 0x02 | Implement |
| `authenticatorGetInfo` | 0x04 | Implement |
| `authenticatorClientPIN` | 0x06 | Future |
| `authenticatorReset` | 0x07 | Future |
| `authenticatorSelection` | 0x0B | Implement |

### CTAP2 Message Flow

```
Browser (WebAuthn)
    ↓ makeCredential / getAssertion
CTAP2 Client (Browser)
    ↓ CBOR-encoded request via USB HID
ESP-FIDO (USB HID Device)
    ↓ Parse CBOR → Execute → Sign → CBOR response
Browser
    ↓ Verify signature with public key
Relying Party (Google, etc.)
```

### Supported Algorithms

| Alg | COSE ID | Curve |
|-----|---------|-------|
| ES256 | -7 | secp256r1 (P-256) |

### Attestation

- **Type**: Self (not packed, no attestation certificate)
- 製品化する場合はattestation証明書の実装が必要

## Pattern Authentication Gate

通常の「ボタン1回押しでユーザーPresence確認」を、特定パターン入力に置き換える。

### Pattern Rule

- **BtnAを1秒以内に4回連続押し** → 承認
- それ以外の入力 → 拒否（無応答またはエラー）
- タイムアウト: 要求受信から**15秒**（ブラウザCTAP2タイムアウト30秒に対する余裕）

### State Machine

```
                    ┌──────────┐
                    │  IDLE    │ ← パターン入力なし、待機中
                    └────┬─────┘
                         │ CTAP2 request受信
                         ▼
                    ┌──────────┐
            ┌──────│ WAITING  │ ← LCD: "WAITING PATTERN..."
            │      └────┬─────┘
            │           │ BtnA press
            │           ▼
            │      ┌──────────┐
            │      │ COUNTING │ ← LCD: "●●●○" (進捗表示)
            │      └────┬─────┘
            │           │
            │    ┌──────┼──────────────┐
            │    │ 4回/1s内  │ タイムアウト/誤入力
            │    ▼           ▼
            │ ┌────────┐  ┌──────────┐
            │ │AUTHED  │  │ REJECTED │
            │ │→署名実行│  │→エラー応答│
            │ └────────┘  └──────────┘
            │    │              │
            └────┴──────────────┘
                    │
                    ▼
              ┌──────────┐
              │  IDLE    │
              └──────────┘
```

### Display States

| State | LCD表示 | 備考 |
|-------|---------|------|
| IDLE | `READY` | 通常待機 |
| WAITING | `WAITING PATTERN...` | CTAP2要求受信後 |
| COUNTING | `●●●○` (押下回数分) | BtnA押下ごとに●が増加 |
| AUTHED | `AUTHENTICATED` (2秒間) | 署名成功後 |
| REJECTED | `DENIED` (2秒間) | パターン不一致/タイムアウト |

### Timeout Behavior

- パターン不一致またはタイムアウト時: CTAP2エラーコード `0x2E` (CTAP2_ERR_ACTION_TIMEOUT) を返す
- ブラウザ側で「セキュリティキーがタイムアウトしました」と表示される
- 再試行はブラウザ側から行われる（ユーザーが再度ログインを試みる）

## Library Selection

| Purpose | Library | Reason |
|---------|---------|--------|
| USB HID | Adafruit TinyUSB | ESP32-S3内蔵USB-OTG対応、HID descriptor柔軟設定 |
| CBOR decode/encode | ArduinoCBOR (qubole) | 軽量CBOR実装、CTAP2メッセージの解析・生成 |
| ECDSA (P-256) | micro-ecc | 最小フットプリントのECCライブラリ、ESP32対応 |
| SHA-256 | ESP32 hardware SHA (mbedTLS) | ESP-IDF内蔵、ハードウェアアクセラレーションあり |
| Display | M5StickC-S3 lib | ST7789 LCD制御、M5GFXベース |
| RNG | esp_random() | ESP32-S3ハードウェアRNG、FIPS準拠 |

## Security Design

### Key Storage

| Key | Storage | Notes |
|-----|---------|-------|
| Master Secret (device-specific) | NVS (encrypted) | 全クレデンシャルの導出元 |
| Credential Private Keys | NVS (encrypted) | RP IDごとに派生鍵を生成 |
| Credential IDs | NVS | 暗号化して保存、RP IDをキーに検索 |

**注意**: ESP32のNVS暗号化はFlash Encryptionに依存する。開発段階ではプレーンNVSでも可だが、実運用時はFlash Encryptionの有効化が必須。

### Key Derivation

```
Master Secret (32B, ランダム生成)
    │
    ├─ HKDF-SHA256(master_secret, rp_id, "cred_key")
    │   → Credential Private Key (32B)
    │
    └─ HKDF-SHA256(master_secret, rp_id, "cred_id")
        → Credential ID (32B, ランダム識別子)
```

この方式の利点: クレデンシャルの秘密鍵をMaster Secretから決定論的に導出できるため、ストレージ容量を節約できる。Master Secretさえバックアップすれば復旧可能。

### Threat Model

| Threat | Mitigation |
|--------|-----------|
| USB経由の鍵抽出 | 秘密鍵はUSB経由で読み出し不可（CTAP2仕様に鍵エクスポートなし） |
| パターンの肩越し盗み見 | LCD表示は最小限、パターン入力後すぐにクリア |
| Flash読み出し | Flash Encryption有効化で対策 |
| リプレイ攻撃 | CTAP2チャレンジ(Challenge)による対策 |
| クローン攻撃 | Master SecretのNVS暗号化 + Flash Encryption |

## Implementation Notes

### Browser Timeout

ブラウザのCTAP2タイムアウトは通常30秒。パターン入力の待ち時間を**15秒**に設定し、残り15秒を署名処理とUSB通信のオーバーヘッドに確保。

### USB HID Initialization Order

ESP32-S3の内蔵USB-OTGを使用する場合、USB初期化は`setup()`の先頭で行う必要がある。TinyUSBのHIDデスクリプタはコンパイル時に固定されるため、動的変更は不可。

### CBOR Encoding

CTAP2のCBORメッセージは簡易的なマップ構造。フルCBORパーサーではなく、CTAP2で使用される限定サブセット（unsigned int, byte string, text string, map, array）のみをサポートする軽量パーサーの自作も選択肢。

### ESP32-S3 Crypto Acceleration

ESP32-S3はP-256のハードウェアアクセラレーションを持つ。micro-eccのソフトウェア実装でも十分な速度だが、ESP-IDFの`esp_ecdsa` peripheralsを使うと更に高速化可能。

### Memory Constraints

- CTAP2のCBORレスポンスは最大1KB程度
- micro-eccの鍵生成・署名に約4KBのスタックが必要
- ESP32-S3の512KB SRAM + 2MB PSRAMで十分に動作可能

## Project Structure (Planned)

```
esp-fido/
├── README.md
├── platformio.ini
├── src/
│   ├── main.cpp                # Entry point, Arduino setup/loop
│   ├── ctap2.h                 # CTAP2 command definitions
│   ├── ctap2.cpp               # CTAP2 request parser & response builder
│   ├── hid_descriptor.h        # USB HID report descriptor
│   ├── authenticator.h         # Authenticator core logic
│   ├── authenticator.cpp        # MakeCredential, GetAssertion, signing
│   ├── pattern_gate.h          # Pattern authentication state machine
│   ├── pattern_gate.cpp        # Pattern detection & user presence
│   ├── cbor_lite.h             # Lightweight CBOR encoder/decoder
│   ├── cbor_lite.cpp
│   ├── crypto_wrapper.h        # ECDSA/SHA256 abstraction
│   ├── crypto_wrapper.cpp      # micro-ecc + esp_sha integration
│   └── key_storage.h          # NVS key storage interface
│   └── key_storage.cpp
├── lib/
│   └── (managed by PlatformIO)
└── test/
    └── (unit tests, future)
```

## Build & Flash

```bash
# PlatformIOでビルド
pio run -e m5stickc-s3

# シリアルポートにフラッシュ
pio run -e m5stickc-s3 -t upload

# シリアルモニタ
pio device monitor
```

## Current Status

- [x] 仕様定義 (this README)
- [ ] USB HID デバイス実装
- [ ] CBOR パーサー/エンコーダ
- [ ] CTAP2 コマンド実装 (MakeCredential, GetAssertion, GetInfo)
- [ ] パターン認証ゲート
- [ ] ECDSA 署名 (micro-ecc)
- [ ] NVS 鍵ストレージ
- [ ] LCD 表示統合
- [ ] ブラウザテスト (Chrome + WebAuthn)
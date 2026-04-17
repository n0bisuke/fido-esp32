# 現在の実装状態

> 最終更新: 2026-04-17

## プロジェクト概要
M5StickC S3 (ESP32-S3) を用いた自作FIDO2/CTAP2セキュリティキー

## 現在のバージョン
- Version: 0.1.0 (開発初期)
- C++ / Arduino framework + PlatformIO
- ESP32-S3 (M5StickC S3)

## 実装済み

### プロジェクト基盤
| 項目 | 状態 | 備考 |
|------|------|------|
| platformio.ini | 完了 | ボード定義、ライブラリ依存、sdkconfig連携 |
| カスタムボード定義 | 完了 | `boards/m5stickc-s3.json` (8MB Flash, 2MB PSRAM) |
| sdkconfig.defaults | 完了 | TinyUSB: HIDのみ有効、他無効化 |
| ビルド成功 | 完了 | RAM 6.3% (20.5KB), Flash 11.4% (382KB), 警告なし |
| .gitignore | 完了 | .pio/, build/, .env, *.bin 等 |
| AGENTS.md / CLAUDE.md | 完了 | 作業ルール・プロジェクト固有注意点 |
| devcontainer | 完了 | pip先行upgrade, libffi-dev等の依存追加、プラットフォーム事前キャッシュ |

### USB HID
| 項目 | 状態 | 備考 |
|------|------|------|
| HIDレポートディスクリプタ | 完了 | `src/hid_descriptor.h` Usage Page 0xF1D0, 64B |
| TinyUSB初期化 | 完了 | `Adafruit_USBD_HID` + OUT endpoint有効 |
| HIDコールバック枠 | 完了 | get_report/set_report 雛形のみ |

### 表示
| 項目 | 状態 | 備考 |
|------|------|------|
| M5GFX初期化 | 完了 | rotation=1, textSize=2 |
| "READY"表示 | 完了 | 起動時にLCD表示 |

## 未実装

### CTAP2プロトコル
- [ ] CBOR パーサー/エンコーダ (tinycbor使用予定)
- [ ] CTAP2 コマンド受信・解析 (set_report_callback内)
- [ ] authenticatorMakeCredential (0x01)
- [ ] authenticatorGetAssertion (0x02)
- [ ] authenticatorGetInfo (0x04)
- [ ] authenticatorSelection (0x0B)

### 暗号・鍵管理
- [ ] ECDSA署名 (micro-ecc, P-256/secp256r1)
- [ ] SHA-256ハッシュ (ESP32-S3ハードウェアアクセラレーション)
- [ ] Master Secret生成・NVS保存
- [ ] 鍵導出 (HKDF-SHA256)
- [ ] クレデンシャルID生成・検索

### パターン認証ゲート
- [ ] 状態遷移 (IDLE→WAITING→COUNTING→AUTHED/REJECTED)
- [ ] BtnA押下検出 (GPIO37, 1秒以内4回)
- [ ] タイムアウト処理 (15秒)
- [ ] LCD進捗表示 ("WAITING PATTERN...", "●●●○", "AUTHENTICATED", "DENIED")

## ファイル構成

```
esp-fido/
├── boards/
│   └── m5stickc-s3.json       # カスタムボード定義
├── docs/
│   └── current-state.md       # このファイル
├── src/
│   ├── main.cpp               # Entry point, USB HID初期化, LCD "READY"
│   └── hid_descriptor.h       # FIDO2 HIDレポートディスクリプタ
├── .devcontainer/
│   ├── devcontainer.json      # VS Code設定, USBデバイスパススルー
│   └── Dockerfile             # PlatformIO + pip + 依存lib事前キャッシュ
├── sdkconfig.defaults          # TinyUSB: HIDのみ有効
├── platformio.ini
├── .gitignore
├── AGENTS.md
├── CLAUDE.md
└── README.md
```

## 既知の注意点
- M5StickC S3はPlatformIOにボード定義がないため、カスタム定義 (`boards/m5stickc-s3.json`) を使用
- ESP32-S3 ArduinoのTinyUSB設定は `sdkconfig.defaults` で制御（`CFG_TUD_*`でなく `CONFIG_TINYUSB_*` を使う）
- `Adafruit_USBD_HID::setReportCallback` は2引数 (get_report, set_report) 必須
- TinyUSBの付属依存（SPIFlash, NeoPixel, SdFat, MIDI）はFIDO2不要。将来lib_depsから除外して軽量化可
- 実機テストはユーザー環境のみ（devcontainerにシリアルデバイスなし）
- ブラウザCTAP2タイムアウト30秒に対し、パターン入力待ちは15秒に設計
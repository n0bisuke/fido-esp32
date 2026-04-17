# 現在の実装状態

> 最終更新: 2026-04-17

## プロジェクト概要
M5Stackデバイス (ESP32-S3) を用いた自作FIDO2/CTAP2セキュリティキー

## 現在のバージョン
- Version: 0.2.0 (GetInfo動作確認済み)
- C++ / Arduino framework + PlatformIO
- マルチターゲット: AtomS3 (プライマリ) / M5StickC S3 (セカンダリ)

## マイルストーン
- [x] USB HID認識 + LCD "READY"表示 (AtomS3で実機確認)
- [x] authenticatorGetInfo動作 (ブラウザがデバイスを検出)
- [ ] authenticatorMakeCredential実装
- [ ] authenticatorGetAssertion実装
- [ ] パターン認証ゲート

## 実装済み

### プロジェクト基盤
| 項目 | 状態 | 備考 |
|------|------|------|
| platformio.ini | 完了 | マルチ環境構成 (atoms3 / m5stickc-s3) |
| カスタムボード定義 | 完了 | `boards/m5stickc-s3.json` (AtomS3は標準ボード使用) |
| sdkconfig.defaults | 完了 | TinyUSB: HIDのみ有効、他無効化 |
| ビルド成功 | 完了 | atoms3: RAM 6.1% (20KB) Flash 10.9% (364KB) / m5stickc-s3: 同規模 |
| .gitignore | 完了 | .pio/, build/, .env, *.bin 等 |
| AGENTS.md / CLAUDE.md | 完了 | 作業ルール・プロジェクト固有注意点 |
| devcontainer | 完了 | pip先行upgrade, libffi-dev等の依存追加、プラットフォーム事前キャッシュ |

### USB HID
| 項目 | 状態 | 備考 |
|------|------|------|
| HIDレポートディスクリプタ | 完了 | `src/hid_descriptor.h` Usage Page 0xF1D0, 64B |
| TinyUSB初期化 | 完了 | `Adafruit_USBD_HID` + OUT endpoint有効 |
| HIDコールバック | 完了 | set_report → ctap2_process_hid_report に接続 |

### CTAP2プロトコル
| 項目 | 状態 | 備考 |
|------|------|------|
| CTAPHIDフレーミング | 完了 | INIT/CONTパケットの組み立て・分解・マルチパケット送受信 |
| CTAPHID_INIT (0x86) | 完了 | チャンネルID割り当て、nonce応答 |
| CTAPHID_MSG (0x83) | 完了 | CTAP2コマンドディスパッチ |
| CTAPHID_PING (0x80) | 完了 | エコーバック |
| authenticatorGetInfo (0x04) | 完了 | **実機動作確認済み** (ブラウザがデバイスを検出) |
| authenticatorMakeCredential (0x01) | 未実装 | CTAP2_ERR_INVALID_COMMANDを返す |
| authenticatorGetAssertion (0x02) | 未実装 | CTAP2_ERR_INVALID_COMMANDを返す |
| authenticatorSelection (0x0B) | 未実装 | CTAP2_ERR_INVALID_COMMANDを返す |

### 表示
| 項目 | 状態 | 備考 |
|------|------|------|
| M5GFX初期化 | 完了 | `#ifdef ATOMS3` / `#ifdef M5STICKC_S3` で分岐 |
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
- [ ] BtnA押下検出 (AtomS3: GPIO41, M5StickC S3: GPIO37, 1秒以内4回)
- [ ] タイムアウト処理 (15秒)
- [ ] LCD進捗表示 ("WAITING PATTERN...", "●●●○", "AUTHENTICATED", "DENIED")

## ファイル構成

```
esp-fido/
├── boards/
│   └── m5stickc-s3.json       # カスタムボード定義 (AtomS3は標準)
├── docs/
│   └── current-state.md       # このファイル
├── src/
│   ├── main.cpp               # Entry point, USB HID初期化, ctap2_init()
│   ├── hid_descriptor.h       # FIDO2 HIDレポートディスクリプタ
│   ├── ctap2.h                 # CTAPHID/CTAP2定数・エラーコード・API
│   ├── ctap2.cpp               # CTAPHIDフレーミング・INIT・ディスパッチ
│   ├── authenticator.h         # AAGUID・GetInfo宣言
│   └── authenticator.cpp        # authenticatorGetInfo (CBOR応答生成)
├── .devcontainer/
│   ├── devcontainer.json      # VS Code設定, USBデバイスパススルー
│   └── Dockerfile             # PlatformIO + pip + 依存lib事前キャッシュ
├── sdkconfig.defaults          # TinyUSB: HIDのみ有効
├── platformio.ini              # マルチ環境 (atoms3 / m5stickc-s3)
├── .gitignore
├── AGENTS.md
├── CLAUDE.md
└── README.md
```

## 既知の注意点
- M5StickC S3はPlatformIOにボード定義がないため、カスタム定義 (`boards/m5stickc-s3.json`) を使用
- AtomS3はPlatformIO標準ボード (`m5stack-atoms3`) を使用、PSRAMなし
- ESP32-S3 ArduinoのTinyUSB設定は `sdkconfig.defaults` で制御（`CFG_TUD_*`でなく `CONFIG_TINYUSB_*` を使う）
- `Adafruit_USBD_HID::setReportCallback` は2引数 (get_report, set_report) 必須
- TinyUSBの付属依存（SPIFlash, NeoPixel, SdFat, MIDI）はFIDO2不要。将来lib_depsから除外して軽量化可
- 実機テストはユーザー環境のみ（devcontainerにシリアルデバイスなし）
- ブラウザCTAP2タイムアウト30秒に対し、パターン入力待ちは15秒に設計
- AtomS3のBtnAはGPIO41 (M5StickC S3はGPIO37)
- GetInfoレスポンス(約74B)はCTAPHIDの57B制限を超えるため2パケット分割送信が必要
- `tud_hid_report_complete_cb` でCONTパケットの継続送信を制御
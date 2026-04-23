# 現在の実装状態

> 最終更新: 2026-04-20

## プロジェクト概要
Seeed Studio XIAO ESP32-S3 を用いた自作FIDO2/CTAP2セキュリティキー

## 現在のバージョン

## マイルストーン
- [ ] USB HID認識 + Serial "READY"出力 (XIAO ESP32-S3で実機確認)
- [ ] authenticatorGetInfo動作 (ブラウザがデバイスを検出)
- [ ] authenticatorMakeCredential実装
- [ ] authenticatorGetAssertion実装
- [ ] パターン認証ゲート

## 実装済み

### プロジェクト基盤
| 項目 | 状態 | 備考 |
|------|------|------|
| PlatformIO設定 | 完了 | env:xiao-esp32s3、TinyUSBモード |
| カスタムボード定義 | 完了 | boards/seeed_xiao_esp32s3-tinyusb.json |
| sdkconfig.defaults | 完了 | TinyUSB HID有効、UARTコンソール、HKDF有効 |
| ビルド | 成功 | RAM 7.1%, Flash 8.9% |

### USB HID
| 項目 | 状態 | 備考 |
|------|------|------|
| HIDディスクリプタ | 完了 | FIDO Alliance Usage Page (0xF1D0) |
| TinyUSB設定 | 完了 | HID有効、CDC/MSC無効 |
| USB初期化 | 完了 | setup()内でbegin |
| set_report_callback | 完了 | CTAP2パケット受信→ctap2_process_hid_report |
| tud_hid_report_complete_cb | 完了 | マルチパケット送信継続 |

### CTAP2
| 項目 | 状態 | 備考 |
|------|------|------|
| CTAPHID INIT | 完了 | nonce echo、CID割り当て、version/capabilities返答 |
| CTAPHID MSG | 完了 | コマンドディスパッチ |
| CTAPHID PING | 完了 | エコーバック |
| CTAPHID ERROR | 完了 | エラーコード返答 |
| マルチパケット受信 | 完了 | INIT/CONTパケット再構築 |
| マルチパケット送信 | 完了 | tud_hid_report_complete_cbで継続送信 |
| authenticatorGetInfo | 完了 | CBOR応答生成 |
| authenticatorSelection | 完了 | 常にtrue |
| authenticatorMakeCredential | 未実装 | pending_reqへ遅延、authenticator側はスタブ |
| authenticatorGetAssertion | 未実装 | pending_reqへ遅延、authenticator側はスタブ |

### 暗号・鍵管理
| 項目 | 状態 | 備考 |
|------|------|------|
| crypto_init | 完了 | micro-ecc初期化なし（必要に応じて） |
| SHA-256 | 完了 | esp_sha()使用 |
| HKDF | 完了 | mbedTLS HKDF |
| ECDSA署名 | 完了 | micro-ecc uECC_sign |
| ECDSA鍵生成 | 完了 | micro-ecc uECC_make_key |
| key_storage_init | 完了 | NVS初期化、Master Secret生成/読込 |
| 鍵導出 | 完了 | HKDF-SHA256でrp_id→cred_key/cred_id |

### 表示
| 項目 | 状態 | 備考 |
|------|------|------|
| Serial出力 | 完了 | Serial Monitorで状態確認 |
| LCD表示 | なし | XIAO ESP32-S3にはLCD非搭載 |

## 未実装
- authenticatorMakeCredential（CBORパーサー、クレデンシャル生成、attestation）
- authenticatorGetAssertion（CBORパーサー、クレデンシャル検索、署名）
- パターン認証ゲート（pattern_gate.h/cpp）
- CBOR軽量パーサー（cbor_lite.h/cpp）
- ブラウザ実機テスト

## 既知の課題
- CBORエンコード/デコードの本格実装が必要（tinycborライブラリ導入済みだがCTAP2向けの統合は未完了）
- MakeCredential/GetAssertionのauthenticator実装がスタブ状態
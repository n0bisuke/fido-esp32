# CLAUDE.md
@AGENTS.md
@README.md

## Claude Code 固有ルール
- 日本語でコミュニケーションすること
- ファイル読み込みは必要な範囲だけにすること（offset/limitを活用してトークンを節約）
- 大きなコード探索はAgent（サブエージェント）に任せ、メインコンテキストを節約すること
- 実装前に EnterPlanMode で方針を合わせること（1行修正レベルは除く）
- sudo は使用しないこと

## プロジェクト固有の注意点
- ESP32-S3の内蔵USB-OTGを使用するため、USB初期化は `setup()` の先頭で行うこと。TinyUSBのHIDディスクリプタはコンパイル時に固定される。
- micro-eccの鍵生成・署名には約4KBのスタックが必要。タスク作成時はスタックサイズに注意すること。
- ブラウザのCTAP2タイムアウトは30秒。パターン入力の待ち時間は15秒以内に収めること。
- ArduinoCBORがCTAP2に必要なCBORサブセットをカバーできない場合は、`cbor_lite.h/cpp` に自作軽量パーサーを実装すること。
- NVSへのアクセスは `nvs_open` / `nvs_set_blob` / `nvs_get_blob` を使うこと。Arduino PreferencesはNVSのラッパーだが、バイナリデータには向かない。
- Flash Encryptionが無効な開発段階ではNVSはプレーン保存でも可だが、本番では有効化が必須であることをコメントで明記すること。
- ESP32-S3のハードウェアSHA-256は `esp_sha()` またはmbedTLS経由で利用可能。ソフトウェアSHAは避けること。
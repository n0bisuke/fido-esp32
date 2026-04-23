# AGENTS.md

## プロジェクト概要
- esp-fido: Seeed Studio XIAO ESP32-S3 を用いた自作FIDO2/CTAP2セキュリティキー
- 現在の目標: Google等のWebAuthnログインに使えるセキュリティキーの実装
- ユーザーが明示した要件の範囲で変更し、要件を勝手に追加しないこと。

## 作業ルール
- 実装や設計を進める前に、まずユーザーの要件を正確に把握すること。
- 実装に影響する前提が不足している場合は、先にヒアリングすること。
- 推測ベースの大きなリファクタではなく、段階的な実装を優先すること。
- ユーザーから依頼がない限り、既存の方向性を勝手に変えないこと。
- 引き継ぎで開発を再開する際は、前回の課題と今回触る範囲を最初に短く確認してから着手すること。
- 着手後の軽微なバグ調整や切り分けは逐一許可待ちにせず、小さく直して検証まで進めること。
- ユーザー要件を広げる変更は避けること。特に未依頼の機能追加は行わないこと。

## コード方針
- C++ (Arduino framework) + PlatformIO を使う。
- 1ファイルあたり約400行以下を目安にすること。大きくなったら責務ごとに分割する。
- 外部ライブラリ: Adafruit TinyUSB, micro-ecc, tinycbor
- コメントは最小限にし、非自明な処理にだけ付けること。
- 秘密情報（Master Secret等）はソースコードにハードコードせず、NVS (encrypted) に保存すること。
- CTAP2仕様に準拠すること。独自拡張はパターン認証ゲートのみ。

## ファイル構成

```
esp-fido/
├── src/
│   ├── main.cpp                # Entry point, Arduino setup/loop
│   ├── ctap2.h                 # CTAP2 command definitions
│   ├── ctap2.cpp               # CTAP2 request parser & response builder
│   ├── hid_descriptor.h        # USB HID report descriptor
│   ├── authenticator.h         # Authenticator core logic
│   ├── authenticator.cpp       # MakeCredential, GetAssertion, signing
│   ├── pattern_gate.h          # Pattern authentication state machine
│   ├── pattern_gate.cpp        # Pattern detection & user presence
│   ├── cbor_lite.h             # Lightweight CBOR encoder/decoder
│   ├── cbor_lite.cpp
│   ├── crypto_wrapper.h        # ECDSA/SHA256 abstraction
│   ├── crypto_wrapper.cpp      # micro-ecc + esp_sha integration
│   └── key_storage.h           # NVS key storage interface
│   └── key_storage.cpp
├── boards/                    # カスタムボード定義
│   └── seeed_xiao_esp32s3-tinyusb.json
├── docs/
│   └── current-state.md       # 実装状態サマリ
├── lib/                        # PlatformIO管理のライブラリ
├── test/                       # ユニットテスト（将来）
├── sdkconfig.defaults          # TinyUSB sdkconfig上書き
├── platformio.ini
├── AGENTS.md
├── CLAUDE.md
└── README.md
```

### 分割ルール
- CTAP2プロトコル処理は `ctap2.h/cpp` にまとめる。
- 認証器ロジック（MakeCredential, GetAssertion）は `authenticator.h/cpp` にまとめる。
- パターン認証ゲートは `pattern_gate.h/cpp` に独立させる。
- 暗号処理のラッパーは `crypto_wrapper.h/cpp` にまとめる。
- 新しいCTAP2コマンド追加時は `authenticator.h/cpp` を更新する。

## パターン認証ゲート仕様
- **承認条件**: BOOTボタン(GPIO0)を1秒以内に4回連続押し
- **タイムアウト**: 要求受信から15秒（ブラウザCTAP2タイムアウト30秒に対する余裕）
- **不一致時**: CTAP2_ERR_ACTION_TIMEOUT (0x2E) を返す
- **Serial表示**: IDLE→READY / WAITING→"WAITING PATTERN..." / COUNTING→"●●●○" / AUTHED→"AUTHENTICATED" / REJECTED→"DENIED"

## ログ・記録ルール
「ログに残して」「記録して」などの指示があった場合、以下を更新すること：
1. `docs/current-state.md` — 実装状態サマリ（必須）
2. `AGENTS.md` のステータス欄 — ステータスが大きく変わった場合のみ

## 現在のステータス
- フェーズ: authenticatorGetInfo動作確認済み、MakeCredential/GetAssertion未実装
- 詳細な実装状態は docs/current-state.md を参照

## 検証
- コード変更後は `pio run -e xiao-esp32s3` でビルドを確認すること。
- 実機テストはユーザー環境で行うこと（devcontainerには実機接続不可）。
- CTAP2の相互接続性はChrome等のブラウザでWebAuthnテストページを使って検証すること。
- 未実装の部分がある場合は、その点を明確に伝えること。
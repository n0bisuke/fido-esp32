FROM mcr.microsoft.com/vscode/devcontainers/python:3.11

# システム依存関係
# - udev: PlatformIOのシリアルポート監視用
# - build-essential: ネイティブライブラリ(micro-ecc等)のコンパイル用
# - libffi-dev, libssl-dev, python3-dev: esptoolpyのcryptography依存のビルド用
#   (pipのhash検証エラーを防ぐため事前インストールが必要)
# - zstd: Ollamaアーカイブ展開用
# - usbutils: lsusb等でデバイス確認用 (実機接続時)
RUN apt-get update && apt-get install -y \
    curl \
    git \
    udev \
    build-essential \
    zstd \
    libffi-dev \
    libssl-dev \
    python3-dev \
    usbutils \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

# 1. PlatformIOのインストール
# pipを最新化してからインストールすることで、
# cryptography等の依存パッケージのhash不一致エラーを防ぐ
USER vscode
RUN pip3 install --user --upgrade pip && \
    pip3 install --user -U platformio

# 2. PlatformIOのグローバルプラットフォーム・パッケージを事前キャッシュ
# ビルド時にダウンロードする時間を省く。espressif32 @ 6.13.0 を想定。
RUN ~/.local/bin/pio pkg install -g -p espressif32@6.13.0

# 3. Ollamaのインストール
USER root
RUN curl -fsSL https://ollama.com/install.sh | sh

# 4. Claude Codeのインストール
RUN npm install -g @anthropic-ai/claude-code

# 権限設定
# dialout: シリアルポート(/dev/ttyUSB*, /dev/ttyACM*)へのアクセス用
# plugdev: USBデバイス(udev)へのアクセス用 (ESP32-S3のUSB-OTGフラッシュ時)
RUN usermod -a -G dialout,plugdev vscode

USER vscode
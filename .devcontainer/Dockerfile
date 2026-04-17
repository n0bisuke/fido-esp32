FROM mcr.microsoft.com/vscode/devcontainers/python:3.11

# 必要なシステムの依存関係（zstdを追加！）
RUN apt-get update && apt-get install -y \
    curl \
    git \
    udev \
    build-essential \
    zstd \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

# 1. PlatformIOのインストール
USER vscode
RUN pip3 install --user -U platformio

# 2. Ollamaのインストール（ここでzstdが使われます）
USER root
RUN curl -fsSL https://ollama.com/install.sh | sh

# 3. Claude Codeのインストール
RUN npm install -g @anthropic-ai/claude-code

# 権限設定
RUN usermod -a -G dialout vscode

USER vscode

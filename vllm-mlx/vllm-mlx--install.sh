#!/usr/bin/env bash

# 1. Update Homebrew and upgrade the core MLX C++ library
echo "==> Updating Homebrew and upgrading mlx library..."
brew install                mlx
brew update && brew upgrade mlx

# 2. Remove the mismatched Python packages
echo "==> Uninstalling old mlx and vllm-mlx wheels..."
python3 -m pip uninstall -y mlx vllm-mlx --break-system-packages

# 3. Reinstall by forcing a local compilation from source
echo "==> Installing vllm-mlx and compiling mlx from source (this may take a minute)..."
python3 -m pip install mlx vllm-mlx openai --no-binary mlx --break-system-packages

# 4. Ensure the Python 3.14 bin directory is available in the current shell session
echo "==> Ensuring binary path is in the current environment..."
export PATH="$HOME/Library/Python/3.14/bin:$PATH"

# 5. Launch the vllm-mlx server
echo "==> Starting the vllm-mlx server..."
vllm-mlx serve mlx-community/Qwen2.5-Coder-14B-Instruct-4bit --port 8000 --continuous-batching

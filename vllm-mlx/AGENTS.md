# Copilot Instructions

This is a feasibility scratch workspace to compare local LLM inference performance across two machines using [vllm-mlx](https://github.com/vllm-project/vllm):

| Machine | Chip | Unified Memory |
|---|---|---|
| MacBook Pro | M3 | 63 GB |
| Mac Studio | M2 | 32 GB |

Model: **`mlx-community/Qwen2.5-Coder-14B-Instruct-4bit`** (~8 GB weights) — chosen as a capable coding model that fits comfortably on both machines.

## Style

Responses should be ruthlessly concise, prioritizing brevity and providing the exact answer immediately without fluff, pleasantries, introductory summaries, or concluding remarks.

## Setup & Usage

**Install / reinstall (handles mlx version mismatches):**
```bash
bash vllm-mlx-install.sh
```
This script: upgrades the Homebrew `mlx` C++ library, removes old Python wheels, recompiles `mlx` from source (required to match the system lib), then starts the server.

**Start the server manually:**
```bash
export PATH="$HOME/Library/Python/3.14/bin:$PATH"  # as-needed
vllm-mlx serve mlx-community/Qwen2.5-Coder-14B-Instruct-4bit --port 8000 --continuous-batching
```

**Test the running server:**
```bash
python3 vllm-mlx--try-it.py
```
Calls `http://localhost:8000/v1` using the OpenAI-compatible API (no auth needed locally).

## Key Details

- **Python path**: Binaries install to `~/Library/Python/3.14/bin` — must be on `$PATH` before running `vllm-mlx`.
- **Source build required**: `mlx` must be compiled from source (`--no-binary mlx`) to avoid ABI mismatches with the Homebrew C++ lib.
- **API compatibility**: The server speaks the OpenAI chat completions API; use the full model ID (e.g. `mlx-community/Qwen2.5-Coder-14B-Instruct-4bit`) and `api_key="not-needed"` when connecting locally.
- **Model**: `mlx-community/Qwen2.5-Coder-14B-Instruct-4bit` (~8 GB, Qwen 2.5 14B coding model).

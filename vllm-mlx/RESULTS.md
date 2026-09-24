# vllm-mlx Feasibility Benchmark

Comparing local LLM inference across two Apple Silicon machines using [vllm-mlx](https://github.com/vllm-project/vllm).

## Model

**`mlx-community/Qwen2.5-Coder-14B-Instruct-4bit`** (~8 GB weights)

## Benchmark Prompt

> Write a Python function that checks if a number is prime, with a docstring and type hints.

## Results

| Metric | MacBook Pro M3 (63 GB) | Mac Studio M2 (32 GB) |
|---|---|---|
| TTFT (mean) | 0.138s ± 0.060s | 0.121s ± 0.005s |
| Decode tok/s | **41.75 ± 0.22** | 40.41 ± 0.27 |
| Total time | 2.60s ± 0.064s | 2.67s ± 0.017s |
| Completion tokens | 103 | 103 |
| Runs | 5 measured + 1 warm-up | 10 measured + 1 warm-up |

Raw results: `results--macbook-pro-m3.json`, `results--mac-studio-m2.json`

**Verdict**: Essentially neck and neck. M3 MBP edges ahead on decode throughput (~3%); M2 Studio has more consistent TTFT. Both machines run the 14B model very comfortably.

## Feasibility for Agentic Coding Use

**GitHub Copilot CLI**: Not configurable — managed cloud service with a fixed backend.

**Claude Code**: Technically possible via a LiteLLM proxy (to translate OpenAI → Anthropic API format), but the practical bottleneck is model capability rather than speed. Qwen2.5-Coder-14B handles single-shot coding tasks well (as demonstrated above), but complex multi-file agentic workflows require Sonnet-class reasoning. The hardware is not the constraint.

## Setup

```bash
bash vllm-mlx-install.sh
```

Then in a new terminal:

```bash
export PATH="$HOME/Library/Python/3.14/bin:$PATH"
vllm-mlx serve mlx-community/Qwen2.5-Coder-14B-Instruct-4bit --port 8000 --continuous-batching
```

Run the benchmark:

```bash
BENCH_MACHINE="macbook-pro-m3" python3 vllm-mlx--benchmarker.py
```

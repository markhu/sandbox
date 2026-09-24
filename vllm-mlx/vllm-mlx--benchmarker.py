#!/usr/bin/env python3
"""
vllm-mlx benchmarker — single-prompt, batch-size-1 throughput measurement.

Usage:
    BENCH_MACHINE="mac-studio-m2" python3 vllm-mlx--benchmarker.py [--runs N]

Results are appended to results--<machine>.json.
"""

import argparse
import json
import os
import re
import statistics
import time
from datetime import datetime, timezone
from openai import OpenAI

# ── Config ────────────────────────────────────────────────────────────────────
BASE_URL   = "http://localhost:8000/v1"
MODEL      = "mlx-community/Qwen2.5-Coder-14B-Instruct-4bit"
PROMPT     = "Write a Python function that checks if a number is prime, with a docstring and type hints."
MAX_TOKENS = 256
SEED       = 42
TEMP       = 0.0
WARMUP     = 1
# ─────────────────────────────────────────────────────────────────────────────

client = OpenAI(base_url=BASE_URL, api_key="not-needed")


def slugify(s: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_-]", "-", s).strip("-")


def run_once() -> dict:
    """Run one inference pass; return timing + token stats."""
    ttft = None
    completion_tokens = None
    t_start = time.perf_counter()

    stream = client.chat.completions.create(
        model=MODEL,
        messages=[{"role": "user", "content": PROMPT}],
        max_tokens=MAX_TOKENS,
        temperature=TEMP,
        seed=SEED,
        stream=True,
        stream_options={"include_usage": True},
    )

    for chunk in stream:
        # Capture TTFT on first chunk that contains actual content
        if ttft is None:
            delta = chunk.choices[0].delta.content if chunk.choices else None
            if delta:
                ttft = time.perf_counter() - t_start

        # Final chunk carries usage
        if chunk.usage:
            completion_tokens = chunk.usage.completion_tokens

    t_total = time.perf_counter() - t_start

    decode_time = t_total - (ttft or 0)
    return {
        "ttft_s":          round(ttft, 4) if ttft else None,
        "total_s":         round(t_total, 4),
        "completion_tokens": completion_tokens,
        "e2e_toks_per_s":  round(completion_tokens / t_total, 2) if completion_tokens else None,
        "decode_toks_per_s": round(completion_tokens / decode_time, 2) if (completion_tokens and decode_time > 0) else None,
    }


def mean_std(values):
    if len(values) < 2:
        return round(values[0], 4), 0.0
    return round(statistics.mean(values), 4), round(statistics.stdev(values), 4)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=5, help="Number of measured runs (default: 5)")
    args = parser.parse_args()

    machine = slugify(os.environ.get("BENCH_MACHINE", "unknown"))
    print(f"Machine : {machine}")
    print(f"Model   : {MODEL}")
    print(f"Prompt  : {PROMPT}")
    print(f"Runs    : {WARMUP} warm-up + {args.runs} measured  |  max_tokens={MAX_TOKENS}  seed={SEED}  temp={TEMP}")
    print()

    # Warm-up (excluded from stats)
    print(f"[warm-up 1/{WARMUP}] ", end="", flush=True)
    run_once()
    print("done")
    print()

    results = []
    for i in range(1, args.runs + 1):
        print(f"[run {i}/{args.runs}] ", end="", flush=True)
        r = run_once()
        results.append(r)
        print(
            f"ttft={r['ttft_s']}s  total={r['total_s']}s  "
            f"tokens={r['completion_tokens']}  "
            f"decode={r['decode_toks_per_s']} tok/s"
        )

    # Aggregate
    print()
    print("=" * 60)

    def agg(key):
        vals = [r[key] for r in results if r.get(key) is not None]
        return mean_std(vals) if vals else (None, None)

    ttft_mean,   ttft_std   = agg("ttft_s")
    total_mean,  total_std  = agg("total_s")
    tok_mean,    tok_std    = agg("completion_tokens")
    e2e_mean,    e2e_std    = agg("e2e_toks_per_s")
    dec_mean,    dec_std    = agg("decode_toks_per_s")

    print(f"TTFT            : {ttft_mean}s  ± {ttft_std}s")
    print(f"Total time      : {total_mean}s  ± {total_std}s")
    print(f"Completion toks : {tok_mean}  ± {tok_std}")
    print(f"E2E tok/s       : {e2e_mean}  ± {e2e_std}")
    print(f"Decode tok/s    : {dec_mean}  ± {dec_std}")
    print("=" * 60)

    # Persist
    record = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "machine": machine,
        "model": MODEL,
        "prompt": PROMPT,
        "max_tokens": MAX_TOKENS,
        "seed": SEED,
        "temperature": TEMP,
        "warmup_runs": WARMUP,
        "measured_runs": args.runs,
        "runs": results,
        "summary": {
            "ttft_mean_s":         ttft_mean,  "ttft_std_s":         ttft_std,
            "total_mean_s":        total_mean, "total_std_s":        total_std,
            "completion_tokens_mean": tok_mean, "completion_tokens_std": tok_std,
            "e2e_toks_per_s_mean": e2e_mean,   "e2e_toks_per_s_std": e2e_std,
            "decode_toks_per_s_mean": dec_mean, "decode_toks_per_s_std": dec_std,
        },
    }

    out_file = f"results--{machine}.json"
    existing = []
    if os.path.exists(out_file):
        with open(out_file) as f:
            existing = json.load(f)
    existing.append(record)
    with open(out_file, "w") as f:
        json.dump(existing, f, indent=2)
    print(f"\nResults appended to {out_file}")


if __name__ == "__main__":
    main()

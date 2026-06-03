#!/usr/bin/env python3

import time
from openai import OpenAI

PROMPT = "Write a Python function that checks if a number is prime, with a docstring and type hints."

client = OpenAI(base_url="http://localhost:8000/v1", api_key="not-needed")

print(f"Prompt: {PROMPT}\n")
print("=" * 60)

chunks = []
ttft = None
t_start = time.perf_counter()

stream = client.chat.completions.create(
    model="mlx-community/Qwen2.5-Coder-14B-Instruct-4bit",
    messages=[{"role": "user", "content": PROMPT}],
    stream=True,
)

for chunk in stream:
    delta = chunk.choices[0].delta.content or ""
    if delta:
        if ttft is None:
            ttft = time.perf_counter() - t_start
        chunks.append(delta)
        print(delta, end="", flush=True)

t_total = time.perf_counter() - t_start
full_response = "".join(chunks)
token_count = len(full_response.split())  # rough proxy; word count ≈ token count

print("\n" + "=" * 60)
print(f"Time to first token : {ttft:.2f}s")
print(f"Total time          : {t_total:.2f}s")
print(f"~Words generated    : {token_count}")
print(f"~Words/sec          : {token_count / (t_total - ttft):.1f}")


#!/usr/bin/env python3

from openai import OpenAI

client = OpenAI(base_url="http://localhost:8000/v1", api_key="not-needed")
r = client.chat.completions.create(model="mlx-community/Qwen2.5-Coder-14B-Instruct-4bit", messages=[{"role": "user", "content": "Hi!"}])
print(r.choices[0].message.content)

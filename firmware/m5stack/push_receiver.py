#!/usr/bin/env python3
"""
push_receiver.py - Minimal HTTP server that logs incoming pushes from the
M5Stick ClickLogger firmware (or anything else) for testing purposes.

Listens on 0.0.0.0:8090 by default.

Endpoints:
  POST /click     - body is logged verbatim (expected: CSV line or JSON),
                    appended to push_received.log with a server-side
                    receive timestamp and the client's source IP.
  POST /snippets  - raw binary body (ClickLogger's /snippets.bin format:
                    repeated "<iso_ts>,<num_samples>\n" headers each
                    followed by num_samples raw little-endian int16 PCM
                    samples), appended as-is (no decoding) to
                    snippets_received.bin for offline analysis.
  GET  /health    - simple liveness check, returns "ok"
  *    (other)    - 404, but still logged

Usage:
  python3 push_receiver.py [port]

Logs to stdout AND to push_received.log (same directory as this script).
Binary snippet data accumulates in snippets_received.bin (same directory).
"""

import sys
import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

LOG_PATH = Path(__file__).parent / "push_received.log"
SNIPPETS_PATH = Path(__file__).parent / "snippets_received.bin"


def log_line(text: str) -> None:
    print(text, flush=True)
    with open(LOG_PATH, "a") as f:
        f.write(text + "\n")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Silence the default stderr access log; we do our own logging below.
        pass

    def _client(self) -> str:
        return self.client_address[0]

    def do_GET(self):
        if self.path == "/health":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"ok\n")
            return
        self.send_response(404)
        self.end_headers()
        log_line(f"[{datetime.datetime.now().isoformat()}] GET {self.path} "
                  f"from {self._client()} -> 404")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length) if length else b""

        if self.path == "/snippets":
            received_at = datetime.datetime.now().isoformat()
            with open(SNIPPETS_PATH, "ab") as f:
                f.write(raw)
            log_line(f"[{received_at}] POST /snippets from {self._client()}: "
                      f"{len(raw)} bytes -> appended to {SNIPPETS_PATH.name}")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.end_headers()
            self.wfile.write(b"received\n")
            return

        body = raw.decode(errors="replace")
        received_at = datetime.datetime.now().isoformat()
        log_line(f"[{received_at}] POST {self.path} from {self._client()}: "
                  f"{body!r}")

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(b"received\n")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8090
    server = ThreadingHTTPServer(("0.0.0.0", port), Handler)
    log_line(f"[{datetime.datetime.now().isoformat()}] push_receiver "
              f"listening on 0.0.0.0:{port}, logging to {LOG_PATH}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

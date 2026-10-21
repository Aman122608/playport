#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Where Playport's "Send log" lands.

The phone POSTs its log to this listener over the local network; nothing else talks to it, and it
writes what it receives to a folder on this machine. Deliberately small: a convenience for handing a
log across, not a service.

    python tools/playport_log_receiver.py                 # 0.0.0.0:8765, writes to Desktop/PlayportLogs
    python tools/playport_log_receiver.py --port 9000 --dir C:/somewhere
    PLAYPORT_LOG_TOKEN=secret python tools/playport_log_receiver.py    # then POST to /log/secret

The wire contract (the app's Settings > About > Send log speaks it):

  GET  /  /log  /log/*  200, body "playport-log-receiver ready\\n" -- the discovery marker the phone
                        sweeps its own /24 for. A server that merely holds the port does not say
                        this, and is not used.
  POST /log              raw bytes (not JSON, not multipart), Content-Length required, up to 40 MiB.
                        400 otherwise. 200 + "saved <name>\\n" on success.

  X-Playport-Build  the app's version-build, e.g. 0.4.0-1 (goes in the file name)
  X-Playport-Note   base64 of the user's UTF-8 message (a header that is not valid base64 is taken
                    as already plain, so a mismatched sender still gets its message across)

The message goes INTO the saved log as a banner rather than beside it: whoever reads the log reads
one file, and the user's own words are the first thing in it.

Privacy: no telemetry, no outbound requests, no third party. The content is the log file the app
already writes, and it is written only when the player taps Send.
"""
from __future__ import annotations

import argparse
import base64
import datetime as _dt
import os
import socket
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

TOKEN = os.environ.get("PLAYPORT_LOG_TOKEN", "").strip()
MAX_BYTES = 40 << 20


def note_from(headers) -> str:
    """The message the user typed beside the log, if there is one."""
    raw = headers.get("X-Playport-Note", "").strip()
    if not raw:
        return ""
    try:
        return base64.b64decode(raw, validate=True).decode("utf-8", "replace").strip()
    except Exception:
        return raw


def lan_address() -> str:
    """This machine's LAN IP, found without sending a packet (it only picks the default route)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


class Handler(BaseHTTPRequestHandler):
    server_version = "playport-log-receiver/1"
    out_dir: Path

    MARKER = "playport-log-receiver ready"

    def _plain(self, code: int, text: str) -> None:
        body = text.encode()
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:                  # the discovery probe, and a check that the address is right
        path = self.path.split("?", 1)[0]
        if path in ("/", "/log") or path.startswith("/log/"):
            self._plain(200, self.MARKER + "\n")
        else:
            self._plain(404, "not found\n")

    def do_POST(self) -> None:
        path = self.path.split("?", 1)[0]
        if TOKEN and path != f"/log/{TOKEN}":
            self._plain(403, "wrong or missing token\n")
            return
        if not (path == "/log" or path.startswith("/log/")):
            self._plain(404, "not found\n")
            return

        try:
            n = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            n = 0
        if n <= 0 or n > MAX_BYTES:            # a log, not an upload endpoint
            self._plain(400, "expected a log body up to 40 MiB\n")
            return
        data = self.rfile.read(n)
        if len(data) != n:
            self._plain(400, "the body ended early\n")
            return

        stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
        build = (self.headers.get("X-Playport-Build", "").strip().replace("/", "-")
                 .replace("\\", "-")[:40])
        out = self.out_dir / f"playport-{stamp}{'-' + build if build else ''}.log"

        note = note_from(self.headers)
        if note:
            banner = ("=== MESSAGE FROM THE USER ===\n" + note + "\n=== END MESSAGE ===\n\n").encode()
            out.write_bytes(banner + data)
        else:
            out.write_bytes(data)

        lines = data.count(b"\n") + (0 if data.endswith(b"\n") else 1)
        print(f"[{stamp}] received {n} bytes ({lines} lines) -> {out}", flush=True)
        if note:
            print("    message: " + note.replace("\n", "\n             "), flush=True)
        else:
            print("    (no message)", flush=True)
        self._plain(200, f"saved {out.name}\n")


def main() -> int:
    ap = argparse.ArgumentParser(description="Receive a log the Playport app sends over the LAN.")
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--dir", default=str(Path.home() / "Desktop" / "PlayportLogs"))
    ap.add_argument("--host", default="0.0.0.0")
    a = ap.parse_args()

    out_dir = Path(a.dir).expanduser()
    out_dir.mkdir(parents=True, exist_ok=True)
    Handler.out_dir = out_dir

    print(f"writing logs to {out_dir}", flush=True)
    print("Playport finds this by itself when the phone is on the same Wi-Fi (Settings > About > Send log);",
          flush=True)
    print(f"type this in by hand only if it cannot:  http://{lan_address()}:{a.port}"
          + (f"/log/{TOKEN}" if TOKEN else "/log"), flush=True)
    if TOKEN:
        print("a token is required (PLAYPORT_LOG_TOKEN is set)", flush=True)
    else:
        print("no token required; anyone on this LAN who knows the port can post to it", flush=True)
    try:
        srv = ThreadingHTTPServer((a.host, a.port), Handler)
    except OSError as exc:
        print(f"could not listen on {a.host}:{a.port}: {exc}", file=sys.stderr)
        return 1
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())

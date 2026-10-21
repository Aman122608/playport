#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Watch the log folder and print each log as it arrives, with the player's message.

    python tools/watch-logs.py [log dir]

Stay running for the whole session; each new file is printed once and remembered in
`<dir>/.announced`, so a restart does not repeat the ones already reported. The polling is
deliberately dumb (a second at a time, sorted names) because the receiver writes a file once and
never appends to it again.

Ctrl-C to stop. Use ping-log.py instead when a single new log should end the process.
"""
from __future__ import annotations

import os
import re
import sys
import time
from pathlib import Path

directory = sys.argv[1] if len(sys.argv) > 1 else str(Path.home() / "Desktop" / "PlayportLogs")
state = os.path.join(directory, ".announced")
BANNER = re.compile(r"=== MESSAGE FROM THE USER ===\n(.*?)\n=== END MESSAGE ===", re.S)


def announced() -> set[str]:
    try:
        with open(state, "r", encoding="utf-8") as fh:
            return {line.strip() for line in fh if line.strip()}
    except OSError:
        return set()


def mark(name: str) -> None:
    try:
        with open(state, "a", encoding="utf-8") as fh:
            fh.write(name + "\n")
    except OSError:
        pass


def report(path: str, name: str) -> None:
    size = os.path.getsize(path) if os.path.exists(path) else 0
    print(f"\n=== {name} ({size:,} bytes) ===", flush=True)
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            head = fh.read(4096)
    except OSError:
        head = ""
    m = BANNER.search(head)
    if m:
        print("MESSAGE FROM THE USER:", flush=True)
        for line in m.group(1).strip().splitlines():
            print("    " + line, flush=True)
    else:
        print("(no message on that one)", flush=True)


print(f"watching {directory}", flush=True)
seen = announced()
while True:
    try:
        names = sorted(n for n in os.listdir(directory) if n.endswith(".log"))
    except OSError:
        names = []
    for n in names:
        if n in seen:
            continue
        time.sleep(0.5)          # let the receiver finish writing it
        report(os.path.join(directory, n), n)
        seen.add(n)
        mark(n)
    time.sleep(2)

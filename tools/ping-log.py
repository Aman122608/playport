#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Ping on a new Playport log -- and carry the player's message with it.

A message typed on the phone is the highest-value part of the payload: it carries the hands-on
sequence and the player's own judgement about which failure this is, neither of which is in the
machine log. The receiver embeds it in the saved log as a banner, so it can never be separated from
the file -- but a reader still has to KNOW to open the file to see it. This ping prints the message
itself.

It prints "(no message on that one)" when the banner is absent, so it is visible that the message
was looked for rather than skipped.

Exits after the first new log, so a watcher that re-arms after each fire sees every one (a process
that watches continuously can miss the second).

usage: ping-log.py [log dir] [baseline]
   baseline "now" (default) ignores what is already on disk; "all" also reports the files already
   there, newest last.
"""
from __future__ import annotations

import os
import re
import sys
import time
from pathlib import Path

directory = sys.argv[1] if len(sys.argv) > 1 else str(Path.home() / "Desktop" / "PlayportLogs")
baseline = sys.argv[2] if len(sys.argv) > 2 else "now"

BANNER = re.compile(r"=== MESSAGE FROM THE USER ===\n(.*?)\n=== END MESSAGE ===", re.S)


def note_of(path: str) -> str | None:
    """The player's own words, from the banner the receiver writes at the top."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            head = fh.read(4096)
    except OSError:
        return None
    m = BANNER.search(head)
    return m.group(1).strip() if m else None


seen = set()
try:
    seen = {n for n in os.listdir(directory) if n.endswith(".log")}
except OSError:
    seen = set()
if baseline == "all":
    seen = set()

while True:
    try:
        names = sorted(n for n in os.listdir(directory) if n.endswith(".log"))
    except OSError:
        names = []
    for n in names:
        if n not in seen:
            path = os.path.join(directory, n)
            # Settle: the receiver may still be writing the file.
            time.sleep(1.5)
            size = os.path.getsize(path) if os.path.exists(path) else 0
            print(f"NEW LOG: {path} ({size:,} bytes)", flush=True)
            note = note_of(path)
            if note:
                print("MESSAGE FROM THE USER (read this first):", flush=True)
                for line in note.splitlines():
                    print("    " + line, flush=True)
            else:
                print("(no message on that one)", flush=True)
            sys.exit(0)
        seen.add(n)
    time.sleep(2)

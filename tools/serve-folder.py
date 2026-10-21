#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Publish a folder of game files over the local network for the phone to pull.

    python tools/serve-folder.py <folder> [port] [bind-address]

The folder is whatever library of pre-built game directories this machine has: the phone asks for
the manifest, then for each file it wants, and drops them at the same relative paths under
Documents/prefix/drive_c/Games (that is C:\\Games in the guest, ContentKit's InstallLayout).

  GET /  /manifest.json   200 application/json:
                          {"name": "<basename>", "files": [{"path": "rel/path", "size": 123}, ...]}
                          every file under the folder, dotfiles and dot-directories skipped.
  GET /f/<path>           the bytes. Accept-Ranges: bytes; Range: bytes=a-b, a- and -n answered with
                          206 + Content-Range, an unsatisfiable range with 416. Content-Type is
                          application/octet-stream throughout: the phone sniffs the manifest, not MIME.
  GET /list               text/plain: a numbered list plus the copy-paste URL for each file.
  HEAD                    the same headers as the GET, no body.

The address it serves on is the address it prints (see serve_address()): binding 0.0.0.0 on a
machine with more than one interface also answers on the slow one, and a client that takes the first
address that answers then downloads over Wi-Fi instead of Ethernet.
"""
from __future__ import annotations

import json
import os
import socket
import sys
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.realpath(sys.argv[1] if len(sys.argv) > 1 else ".")
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8642
BIND = sys.argv[3] if len(sys.argv) > 3 else ""


def manifest() -> dict:
    files = []
    for base, dirs, names in os.walk(ROOT):
        dirs[:] = sorted(d for d in dirs if not d.startswith("."))
        for n in sorted(names):
            if n.startswith("."):
                continue
            full = os.path.join(base, n)
            try:
                size = os.path.getsize(full)
            except OSError:
                continue
            files.append({"path": os.path.relpath(full, ROOT).replace(os.sep, "/"), "size": size})
    return {"name": os.path.basename(ROOT) or ROOT, "files": files}


def lan_address() -> str:
    """The interface a LAN route would use. No packet is sent; this only reads the routing table."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.0.2.1", 9))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def serve_address() -> str:
    """The address to serve on -- the one this script prints, on purpose (see the module docstring)."""
    return BIND or lan_address()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        sys.stderr.write("%s %s\n" % (self.address_string(), fmt % args))

    def do_HEAD(self) -> None:
        self.do_GET(head=True)

    def _body(self, code: int, body: bytes, mime: str, head: bool) -> None:
        self.send_response(code)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if not head:
            self.wfile.write(body)

    def do_GET(self, head: bool = False) -> None:
        path = urllib.parse.unquote(urllib.parse.urlparse(self.path).path)

        if path in ("/", "/manifest.json"):
            self._body(200, json.dumps(manifest()).encode(), "application/json", head)
            return

        if path == "/list":
            files = sorted(manifest()["files"], key=lambda f: f["path"])
            host = serve_address()
            port = self.server.server_address[1]
            lines = ["%d. %s (%.1f MB)" % (i, f["path"], f["size"] / 1e6)
                     for i, f in enumerate(files, 1)]
            lines += ["", "copy-paste URLs (the phone's import takes any of these):", ""]
            lines += ["  http://%s:%d/f/%s" % (host, port,
                                               "/".join(urllib.parse.quote(p) for p in f["path"].split("/")))
                      for f in files]
            self._body(200, ("\n".join(lines) + "\n").encode(), "text/plain; charset=utf-8", head)
            return

        if not path.startswith("/f/"):
            self.send_error(404)
            return

        rel = path[3:]
        # The manifest skips dotfiles and dot-directories, so a download refuses them too: what is
        # published is what is listed, and nothing here can walk out of the folder.
        if any(part.startswith(".") for part in rel.split("/")):
            self.send_error(404)
            return
        full = os.path.realpath(os.path.join(ROOT, rel))
        if not full.startswith(ROOT + os.sep) or not os.path.isfile(full):
            self.send_error(404)
            return

        size = os.path.getsize(full)
        start, end = 0, size - 1
        rng = self.headers.get("Range")
        if rng and rng.startswith("bytes="):
            spec = rng[6:].split(",")[0]
            a, _, b = spec.partition("-")
            try:
                if a:
                    start = int(a)
                    end = int(b) if b else size - 1
                elif b:
                    start = max(0, size - int(b))
                else:
                    raise ValueError
            except ValueError:
                self.send_response(416)
                self.send_header("Content-Range", "bytes */%d" % size)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            if start >= size or start > end:
                self.send_response(416)
                self.send_header("Content-Range", "bytes */%d" % size)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            end = min(end, size - 1)
            self.send_response(206)
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, end, size))
        else:
            self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(end - start + 1))
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
        if head:
            return
        self.wfile.flush()

        t0, sent = time.time(), 0
        with open(full, "rb") as f:
            # socket.sendfile copies from the file to the socket, with the kernel's sendfile where
            # the platform has one. os.sendfile is Unix-only: on Windows it raises AttributeError
            # and every download dies, which the client reports as "connection lost".
            sock = self.connection
            offset, left = start, end - start + 1
            try:
                while left > 0:
                    n = sock.sendfile(f, offset, min(left, 64 << 20))
                    if not n:
                        break
                    offset += n
                    left -= n
                    sent += n
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
        dt = max(time.time() - t0, 0.001)
        sys.stderr.write("  %s: %.0f MB in %.0f s, %.1f MB/s\n" % (rel, sent / 1e6, dt, sent / 1e6 / dt))


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def server_bind(self):
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 8 << 20)
        super().server_bind()


def main() -> int:
    if not os.path.isdir(ROOT):
        print("not a folder: %s" % ROOT, file=sys.stderr)
        return 2
    m = manifest()
    total = sum(f["size"] for f in m["files"])
    print("Serving %s: %d files, %.1f GB" % (ROOT, len(m["files"]), total / 1e9))
    host = serve_address()
    print("On the phone, this is the folder's address:  http://%s:%d/" % (host, PORT))
    print("Its manifest:  http://%s:%d/manifest.json   Every file:  http://%s:%d/list"
          % (host, PORT, host, PORT))
    try:
        server = Server((host, PORT), Handler)
    except OSError as exc:
        # The address can move between the check above and the bind (a DHCP re-lease, a Wi-Fi drop).
        # 0.0.0.0 keeps the folder reachable, at the cost of answering on interfaces we did not
        # advertise -- say so rather than failing, because a server nobody can reach is worse.
        if BIND:
            print("could not bind %s: %s" % (host, exc), file=sys.stderr)
            return 1
        print("Could not bind %s: %s" % (host, exc))
        print("Serving on 0.0.0.0 instead: every interface, including any slow one.")
        server = Server(("0.0.0.0", PORT), Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())

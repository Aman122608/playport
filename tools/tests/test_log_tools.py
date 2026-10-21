# SPDX-License-Identifier: GPL-3.0-or-later
"""The two PC-side tools the phone talks to: the wire contract it depends on.

`tools/playport_log_receiver.py` (the discovery marker, a raw POST, the message banner) and
`tools/serve-folder.py` (the manifest, byte-range downloads, the traversal guard). Both are
threaded servers bound to 127.0.0.1:0 here, so the test never holds the real ports.
"""

import base64
import importlib.util
import json
import os
import pathlib
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.parse
import urllib.request
from http.server import ThreadingHTTPServer

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def load(name, path, argv=None):
    """A tool's module, with `sys.argv` as its command line if it reads one at import."""
    saved = sys.argv
    if argv is not None:
        sys.argv = argv
    try:
        spec = importlib.util.spec_from_file_location(name, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        sys.argv = saved


def serve(server):
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server.server_address[1]


def get(url, headers=None, method=None, data=None):
    request = urllib.request.Request(url, headers=headers or {}, method=method, data=data)
    try:
        with urllib.request.urlopen(request, timeout=5) as response:
            return response.status, dict(response.headers), response.read()
    except urllib.error.HTTPError as error:
        return error.code, dict(error.headers), error.read()


class LogReceiver(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.mod = load("playport_log_receiver",
                        os.path.join(REPO, "tools", "playport_log_receiver.py"))
        self.mod.Handler.out_dir = pathlib.Path(self.dir.name)
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), self.mod.Handler)
        self.port = serve(self.server)
        self.base = "http://127.0.0.1:%d" % self.port

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.dir.cleanup()

    def testThePhoneFindsTheReceiverByItsMarker(self):
        for path in ("/", "/log", "/log/"):
            status, _, body = get(self.base + path)
            self.assertEqual(status, 200, path)
            self.assertEqual(body, b"playport-log-receiver ready\n", path)
        self.assertEqual(get(self.base + "/elsewhere")[0], 404)

    def testALogArrivesWithTheMessageAtTheTop(self):
        note = "Hollow Knight freezes on the second boss\nTried twice, same spot."
        status, _, body = get(
            self.base + "/log",
            headers={"Content-Type": "text/plain; charset=utf-8",
                     "X-Playport-Build": "0.4.0-1",
                     "X-Playport-Note": base64.b64encode(note.encode()).decode()},
            method="POST", data=b"line one\nline two\n")
        self.assertEqual(status, 200)
        self.assertTrue(body.startswith(b"saved playport-"), body)
        saved = os.listdir(self.dir.name)
        self.assertEqual(len(saved), 1)
        self.assertTrue(saved[0].endswith("-0.4.0-1.log"), saved)
        with open(os.path.join(self.dir.name, saved[0]), "rb") as fh:
            written = fh.read()
        self.assertTrue(written.startswith(b"=== MESSAGE FROM THE USER ===\n"), written[:80])
        self.assertIn(note.encode(), written)
        self.assertIn(b"\n=== END MESSAGE ===\n\n", written)

    def testANoteThatIsNotBase64IsTakenAsPlainAndAnEmptyBodyIsRefused(self):
        status, _, body = get(self.base + "/log", method="POST")
        self.assertEqual(status, 400)            # no Content-Length: a log, not an upload
        self.assertIn(b"40 MiB", body)

        status, _, _ = get(self.base + "/log", headers={"X-Playport-Note": "plain words"},
                           method="POST", data=b"a log line\n")
        self.assertEqual(status, 200)
        with open(os.path.join(self.dir.name, sorted(os.listdir(self.dir.name))[-1]), "rb") as fh:
            self.assertIn(b"plain words", fh.read())


class FolderServer(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        root = os.path.join(self.dir.name, "games", "GameA")
        os.makedirs(root)
        self.payload = bytes(range(256)) * 400        # 102400 bytes
        with open(os.path.join(root, "data.bin"), "wb") as fh:
            fh.write(self.payload)
        with open(os.path.join(root, ".hidden"), "wb") as fh:
            fh.write(b"no")
        self.mod = load("serve_folder", os.path.join(REPO, "tools", "serve-folder.py"),
                        argv=["serve-folder.py", os.path.join(self.dir.name, "games"), "0", "127.0.0.1"])
        self.server = self.mod.Server(("127.0.0.1", 0), self.mod.Handler)
        self.base = "http://127.0.0.1:%d" % serve(self.server)

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.dir.cleanup()

    def testTheManifestListsEveryFileAndSkipsDotfiles(self):
        status, headers, body = get(self.base + "/manifest.json")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Type"], "application/json")
        manifest = json.loads(body)
        self.assertEqual(manifest["name"], "games")
        self.assertEqual(manifest["files"], [{"path": "GameA/data.bin", "size": len(self.payload)}])
        self.assertEqual(get(self.base + "/")[2], body)

    def testADownloadIsTheFileAndARangeIsThePart(self):
        status, headers, body = get(self.base + "/f/GameA/data.bin")
        self.assertEqual(status, 200)
        self.assertEqual(body, self.payload)
        self.assertEqual(headers["Content-Type"], "application/octet-stream")
        self.assertEqual(headers["Accept-Ranges"], "bytes")
        self.assertEqual(headers["Content-Length"], str(len(self.payload)))

        status, headers, part = get(self.base + "/f/GameA/data.bin", headers={"Range": "bytes=1000-1499"})
        self.assertEqual(status, 206)
        self.assertEqual(headers["Content-Range"], "bytes 1000-1499/%d" % len(self.payload))
        self.assertEqual(part, self.payload[1000:1500])

        status, headers, tail = get(self.base + "/f/GameA/data.bin", headers={"Range": "bytes=-10"})
        self.assertEqual(status, 206)
        self.assertEqual(tail, self.payload[-10:])

        status, headers, _ = get(self.base + "/f/GameA/data.bin", headers={"Range": "bytes=%d-" % len(self.payload)})
        self.assertEqual(status, 416)
        self.assertEqual(headers["Content-Range"], "bytes */%d" % len(self.payload))

    def testHeadMirrorsTheGetAndTheFolderCannotBeLeft(self):
        status, headers, body = get(self.base + "/f/GameA/data.bin", method="HEAD")
        self.assertEqual(status, 200)
        self.assertEqual(body, b"")
        self.assertEqual(headers["Content-Length"], str(len(self.payload)))

        self.assertEqual(get(self.base + "/f/../logs/x.log")[0], 404)
        self.assertEqual(get(self.base + "/f/GameA/.hidden")[0], 404)
        self.assertEqual(get(self.base + "/f/nothing.bin")[0], 404)

    def testTheListNamesEveryFileWithItsUrl(self):
        status, headers, body = get(self.base + "/list")
        self.assertEqual(status, 200)
        self.assertTrue(headers["Content-Type"].startswith("text/plain"), headers["Content-Type"])
        text = body.decode()
        self.assertIn("1. GameA/data.bin", text)
        self.assertIn("/f/GameA/data.bin", text)


if __name__ == "__main__":
    unittest.main()

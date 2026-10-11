#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Wine Mono's notices (decision 0077), from its source release.

  notices-wine-mono.py collect SOURCE OUT    copy the notices into the notice assembly OUT
  notices-wine-mono.py verify SOURCE OUT     recompute them and check OUT's copies

SOURCE is wine-mono-<pin>-src.tar.xz, the source release of the shipped
Runtime/mono tree (build/stages/wine-mono.sh fetches it into the cache and holds
its sha256, SRC_SHA256, which this checks first). The binary release carries no
licence files; its COPYING names the licences of each part (Mono, its external
libraries, FNA and its libraries, winforms, wpf, monoDX, System.Speech), and those
parts keep theirs in their directories. Collected is a superset: every regular
member whose name is a licence, copying, notice, patents or third-party file, and
the ICSharpCode.SharpZipLib README COPYING points to for that library's GPL
exception, except under the llvm-mingw toolchain directory the source release
carries to build with (the runtime code it puts into Mono's DLLs is the
llvm-mingw-runtime and mingw-crt parts). Each is copied byte for byte as
winemono-source-<path with / as ->, and winemono-provenance.json records the
archive's sha256 and each member's path, size and sha256.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path
import tarfile

REPO = Path(__file__).resolve().parent.parent
NOTICE = re.compile(r"^(licen[cs]e|copying|notices?|patents|third[-_]?party[-_]?(notices|readme)?)([-._][^/]*)?$", re.I)
EXTRA = {"mono/mcs/class/ICSharpCode.SharpZipLib/README"}
PREFIX = "winemono-source-"
MANIFEST = "winemono-provenance.json"


def pin():
    for line in (REPO / "pins.lock").read_text().splitlines():
        f = line.split()
        if f and f[0] == "wine-mono":
            return f[1]
    raise ValueError("pins.lock has no wine-mono")


def locked_sha256():
    m = re.search(r"^SRC_SHA256=([0-9a-f]{64})$", (REPO / "build/stages/wine-mono.sh").read_text(), re.M)
    if not m:
        raise ValueError("build/stages/wine-mono.sh has no SRC_SHA256")
    return m.group(1)


def file_digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def notices(source):
    """{payload name: (member path, bytes)} from the checked source archive."""
    if file_digest(source) != locked_sha256():
        raise ValueError(f"{source} is not the locked Wine Mono source release")
    root = f"wine-mono-{pin()}/"
    found = {}
    with tarfile.open(source, mode="r|xz") as tar:
        for member in tar:
            if not member.isfile() or not member.name.startswith(root):
                continue
            rel = member.name[len(root):]
            if rel.startswith("llvm-mingw-") or not (NOTICE.match(rel.rsplit("/", 1)[-1]) or rel in EXTRA):
                continue
            if any(p in ("", ".", "..") for p in rel.split("/")) or "\\" in rel:
                raise ValueError(f"unsafe archive member: {member.name}")
            name = PREFIX + rel.replace("/", "-")
            if name in found:
                raise ValueError(f"two notices are both {name}")
            found[name] = (rel, tar.extractfile(member).read())
    missing = EXTRA - {rel for rel, _ in found.values()}
    if "COPYING" not in {rel for rel, _ in found.values()} or missing:
        raise ValueError(f"source release lacks COPYING or {sorted(missing)}")
    return found


def manifest(found):
    return {"schema": 1, "component": "wine-mono", "pin": pin(), "archive": f"wine-mono-{pin()}-src.tar.xz",
            "archive_sha256": locked_sha256(),
            "files": {name: {"member": rel, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                      for name, (rel, data) in sorted(found.items())}}


def collect(source, out):
    found = notices(source)
    for name in list(found) + [MANIFEST]:
        if (out / name).exists() or (out / name).is_symlink():
            raise ValueError(f"duplicate notice output: {name}")
    for name, (_rel, data) in found.items():
        (out / name).write_bytes(data)
    (out / MANIFEST).write_text(json.dumps(manifest(found), indent=2, sort_keys=True) + "\n")
    print(f"wine-mono notices: {len(found)} files")


def verify(source, out):
    expected = manifest(notices(source))
    if json.loads((out / MANIFEST).read_text()) != expected:
        raise ValueError("Wine Mono notice manifest differs from its source release")
    actual = {p.name for p in out.iterdir() if p.name.startswith(PREFIX)}
    if actual != set(expected["files"]):
        raise ValueError("Wine Mono notice payload set differs from its manifest")
    for name, record in expected["files"].items():
        path = out / name
        if path.is_symlink() or file_digest(path) != record["sha256"]:
            raise ValueError(f"Wine Mono notice changed after collection: {name}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("collect", "verify"))
    ap.add_argument("source", type=Path)
    ap.add_argument("out", type=Path)
    a = ap.parse_args()
    try:
        (collect if a.command == "collect" else verify)(a.source, a.out)
    except (OSError, ValueError, tarfile.TarError) as exc:
        ap.exit(1, f"Wine Mono notices: {exc}\n")


if __name__ == "__main__":
    main()

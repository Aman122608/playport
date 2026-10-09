#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Rewrite an IPA with its files deflated, in place (PLA-146).

  build/deflate-ipa.py IPA [--level N]

xtool writes the IPA as a stored zip. Deflating it makes the download smaller (about
1.5 GB to under 0.3 GB) without changing how it is installed or what is installed:
the code signature covers the bundle's files, not the zip around them. Entry order,
names, timestamps and modes are kept, so the same input gives the same output.
Directories stay stored, as zip writes them. The rewrite goes to a temporary file
beside the IPA and replaces it only once complete.
"""
import argparse
import os
import shutil
import sys
import zipfile
from pathlib import Path


def deflate(src: Path, level: int) -> None:
    tmp = src.with_name(src.name + ".deflate.tmp")
    try:
        with zipfile.ZipFile(src) as zin, \
                zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=level) as zout:
            for info in zin.infolist():
                out = zipfile.ZipInfo(info.filename, info.date_time)
                out.external_attr = info.external_attr
                out.create_system = info.create_system
                out.comment = info.comment
                if info.is_dir():
                    out.compress_type = zipfile.ZIP_STORED
                    zout.writestr(out, b"")
                    continue
                out.compress_type = zipfile.ZIP_DEFLATED
                out.file_size = info.file_size
                with zin.open(info) as r, zout.open(out, "w", force_zip64=info.file_size >= 1 << 31) as w:
                    shutil.copyfileobj(r, w, 1 << 22)
        os.replace(tmp, src)
    finally:
        tmp.unlink(missing_ok=True)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("ipa", type=Path)
    p.add_argument("--level", type=int, default=9, choices=range(1, 10), metavar="N",
                   help="zlib level 1-9 (default 9)")
    a = p.parse_args()
    before = a.ipa.stat().st_size
    deflate(a.ipa, a.level)
    print(f"deflate-ipa: {a.ipa.name} {before / 1e6:,.1f} MB -> {a.ipa.stat().st_size / 1e6:,.1f} MB")
    return 0


if __name__ == "__main__":
    sys.exit(main())

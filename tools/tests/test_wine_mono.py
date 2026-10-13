# SPDX-License-Identifier: GPL-3.0-or-later
"""Wine Mono (decision 0077): one version and one pair of sha256s wherever it is
named, staged where the wine pin's mscoree looks, packaged, verified, noticed, and
the support package's registry in the prefix seed."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import tarfile
import tempfile
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, REPO / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


stage = load("mono_stage", "build/stages/stage-artifacts.py")
notices = load("mono_notices", "build/notices-wine-mono.py")
SCRIPT = (REPO / "build/stages/wine-mono.sh").read_text()


class WineMono(unittest.TestCase):
    def test_pin_stage_script_and_source_catalog_agree(self):
        version = stage.pin("wine-mono")
        self.assertRegex(version, r"^\d+\.\d+\.\d+$")
        self.assertEqual(stage.WINE_MONO, version)
        src = re.search(r"^SRC_SHA256=([0-9a-f]{64})$", SCRIPT, re.M).group(1)
        self.assertRegex(SCRIPT, r"(?m)^SHA256=[0-9a-f]{64}$")
        entry = next(f for f in json.loads((REPO / "build/source-bundle.json").read_text())["files"]
                     if f["name"] == "wine-mono")
        self.assertEqual((entry["tag"], entry["sha256"]), (version, src))
        self.assertEqual(entry["file"], f"wine-mono-{version}-src.tar.xz")
        self.assertEqual(entry["url"], f"https://dl.winehq.org/wine/wine-mono/{version}/wine-mono-{version}-src.tar.xz")
        self.assertEqual(notices.locked_sha256(), src)

    def test_the_wine_pins_mscoree_expects_the_pinned_version(self):
        header = Path(stage.RUN) / "pe/wine/dlls/mscoree/mscoree_private.h"
        if not header.is_file():
            self.skipTest(f"no Wine tree at {header.parent}")
        want = re.search(r'#define WINE_MONO_VERSION "([^"]+)"', header.read_text()).group(1)
        self.assertEqual(want, stage.pin("wine-mono"))

    def test_staged_where_mscoree_looks_packaged_verified_and_noticed(self):
        version = stage.pin("wine-mono")
        self.assertEqual(stage.ROOTS["wine-mono"].parts[-2:], (f"wine-mono-{version}", f"wine-mono-{version}"))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "bin").mkdir()
            (root / "lib/mono/4.5").mkdir(parents=True)
            (root / "bin/libmono-2.0-x86_64.dll").write_bytes(b"MZ")
            (root / "lib/mono/gac.dll").write_bytes(b"MZ gac")
            (root / "lib/mono/4.5/link.dll").symlink_to("../gac.dll")
            (root / ".unpacked-from").write_text("x\n")
            with patch.dict(stage.ROOTS, {"wine-mono": root}):
                self.assertEqual(stage.wine_mono_files(),
                                 ["bin/libmono-2.0-x86_64.dll", "lib/mono/4.5/link.dll", "lib/mono/gac.dll"])
        text = (REPO / "app/xtool.yml").read_text()
        self.assertIn("  - Sources/S1Probe/Runtime/mono\n", text)
        self.assertIn("  - Runtime/mono\n", stage.release_xtool_yml(text))
        self.assertIn('"mono"]', (REPO / "build/verify-ipa.py").read_text())
        selection = json.loads((REPO / "build/app-notices.json").read_text())
        self.assertTrue(any("wine-mono" in c["covers"] and c["include"] == ["winemono-source-*"]
                            for c in selection["components"]))
        rows = [l.split("\t") for l in (REPO / "app/artifacts.tsv").read_text().splitlines()
                if l and not l.startswith("#")]
        mono = [r for r in rows if r[6] == "wine-mono"]
        self.assertIn(f"Runtime/mono/wine-mono-{version}/bin/libmono-2.0-x86_64.dll", {r[1] for r in mono})
        self.assertIn(f"Runtime/mono/wine-mono-{version}/bin/libmono-2.0-x86.dll", {r[1] for r in mono})
        self.assertTrue(all(r[0] == "resource" and r[4] == "wine-mono" for r in mono))

    def test_prefix_seed_reports_net_4_installed_for_a_burn_netfx_check(self):
        text = (REPO / "app/registry/system.reg").read_text()
        key = "[Software\\\\Microsoft\\\\NET Framework Setup\\\\NDP\\\\v4\\\\Full]"
        block = text[text.index(key):].split("\n\n", 1)[0]
        self.assertEqual(text[:text.index(key)].rstrip("\n").rsplit("\n", 1)[-1], ";; playport:top-up")
        self.assertIn('"Install"=dword:00000001', block)
        self.assertIn('"Release"=dword:00082348', block)
        self.assertIn("[Software\\\\Microsoft\\\\NET Framework Setup\\\\NDP\\\\v2.0.50727]", text)


class Notices(unittest.TestCase):
    def test_collects_licence_files_and_the_sharpziplib_readme_but_not_code_or_the_toolchain(self):
        version = stage.pin("wine-mono")
        members = {
            "COPYING": b"Wine Mono contains code from several projects",
            "mono/LICENSE": b"MIT",
            "mono/mcs/class/ICSharpCode.SharpZipLib/README": b"GPL with exception",
            "wpf/THIRD-PARTY-NOTICES.TXT": b"notices",
            "mono/mcs/class/System/LicenseManager.cs": b"class LicenseManager {}",
            "llvm-mingw-20260616-ucrt-ubuntu-22.04-x86_64/LICENSE.TXT": b"toolchain",
        }
        with tempfile.TemporaryDirectory() as tmp:
            source, out = Path(tmp) / "src.tar.xz", Path(tmp) / "out"
            out.mkdir()
            with tarfile.open(source, "w:xz") as tar:
                for rel, data in members.items():
                    info = tarfile.TarInfo(f"wine-mono-{version}/{rel}")
                    info.size = len(data)
                    tar.addfile(info, io.BytesIO(data))
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            with patch.object(notices, "locked_sha256", return_value=digest):
                notices.collect(source, out)
                notices.verify(source, out)
                self.assertEqual(sorted(p.name for p in out.iterdir()), [
                    "winemono-provenance.json", "winemono-source-COPYING",
                    "winemono-source-mono-LICENSE",
                    "winemono-source-mono-mcs-class-ICSharpCode.SharpZipLib-README",
                    "winemono-source-wpf-THIRD-PARTY-NOTICES.TXT"])
                (out / "winemono-source-mono-LICENSE").write_bytes(b"changed")
                with self.assertRaises(ValueError):
                    notices.verify(source, out)
            with self.assertRaises(ValueError):
                notices.collect(source, Path(tmp) / "out")


if __name__ == "__main__":
    unittest.main()

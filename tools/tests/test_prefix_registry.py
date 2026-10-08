# SPDX-License-Identifier: GPL-3.0-or-later
"""app/tools/prefix-registry.py's NEUTRALISE keys (decision 0070) in the
committed user.reg seed: marked for top-up, with the values that keep winedbg's
crash dialog, winemenubuilder and control.exe's add-on download off the phone."""
from pathlib import Path
import unittest

REPO = Path(__file__).resolve().parents[2]
MARKER = ";; playport:top-up"


def sections(text):
    """[key] -> (marked, value lines) of a wineserver registry file."""
    out, key, marked, prev = {}, None, False, ""
    for line in text.splitlines():
        if line.startswith("["):
            key, marked = line[1:line.rindex("]")], prev == MARKER
            out[key] = (marked, [])
        elif key and line and not line.startswith(("#", ";")):
            out[key][1].append(line)
        prev = line
    return out


class Neutralise(unittest.TestCase):
    def test_user_seed_turns_off_the_crash_dialog_menubuilder_and_addon_installer(self):
        user = sections((REPO / "app/registry/user.reg").read_text())
        marked, values = user["Software\\\\Wine\\\\WineDbg"]
        self.assertTrue(marked)
        self.assertIn('"ShowCrashDialog"=dword:00000000', values)
        marked, values = user["Software\\\\Wine\\\\DllOverrides"]
        self.assertTrue(marked)
        self.assertIn('"control.exe"=""', values)
        self.assertIn('"winemenubuilder.exe"=""', values)


if __name__ == "__main__":
    unittest.main()

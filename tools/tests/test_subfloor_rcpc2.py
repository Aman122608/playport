# SPDX-License-Identifier: GPL-3.0-or-later
"""The LRCPC2 unscaled decoder the sub-floor service uses (patches/madeira-unix 0122)."""
from pathlib import Path
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
PATCH = REPO / "patches/madeira-unix/0122-signal-emulate-LRCPC2-s-unscaled-loads-and-stores-in.patch"


class SubfloorRCpc2(unittest.TestCase):
    def test_decoder(self):
        added = "\n".join(line[1:] for line in PATCH.read_text().splitlines()
                          if line.startswith("+") and not line.startswith("+++"))
        code = "static int ios_rcpc2_unscaled(" + added.split(
            "static int ios_rcpc2_unscaled(", 1)[1].split("\n}", 1)[0] + "\n}\n"
        # The three users: the load emulator, the release-store emulator, the classifier.
        self.assertEqual(added.count("ios_rcpc2_unscaled( insn,"), 3)
        work = REPO / ".work"
        work.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="subfloor-rcpc2-", dir=work) as tmp:
            root = Path(tmp)
            source = root / "test.c"
            source.write_text("#include <assert.h>\n#include <stdint.h>\n" + code + r'''
static int check(uint32_t insn, unsigned want_bytes, int want_opc)
{
    unsigned bytes = 99;
    int opc = 99;
    int ok = ios_rcpc2_unscaled(insn, &bytes, &opc);
    if (!want_bytes) return !ok && bytes == 99 && opc == 99;
    return ok && bytes == want_bytes && opc == want_opc;
}

int main(void)
{
    /* Football Manager 2022's read of KUSER_SHARED_DATA+0xc: LDAPUR w6, [x29] */
    assert(check(0x994003a6, 4, 1));
    assert(check(0xd9400000 | (0x1f8 << 12) | (2 << 5) | 3, 8, 1)); /* LDAPUR x3, [x2, #-8] */
    assert(check(0x19400000, 1, 1));     /* LDAPURB  */
    assert(check(0x59c00000, 2, 3));     /* LDAPURSH w */
    assert(check(0x19800000, 1, 2));     /* LDAPURSB x */
    assert(check(0x99800000, 4, 2));     /* LDAPURSW */
    assert(check(0x99000000, 4, 0));     /* STLUR w  */
    assert(check(0xd9000000, 8, 0));     /* STLUR x  */
    assert(check(0x19000000, 1, 0));     /* STLURB   */
    /* unallocated */
    assert(check(0xd9800000, 0, 0));
    assert(check(0xd9c00000, 0, 0));
    assert(check(0x99c00000, 0, 0));
    /* other classes */
    assert(check(0xb8400000, 0, 0));     /* LDUR w   */
    assert(check(0xb8bfc000, 0, 0));     /* LDAPR w  */
    assert(check(0x88dffc00, 0, 0));     /* LDAR w   */
    assert(check(0x99400400, 0, 0));     /* bits 11:10 set */
    assert(check(0x99600000, 0, 0));     /* bit 21 set */
    return 0;
}
''')
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined,address", str(source), "-o", str(root / "test")], check=True)
            subprocess.run([str(root / "test")], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()

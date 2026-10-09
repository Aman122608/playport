# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the self-read adapter shipped in the iOS ntdll patch (mock Mach)."""
from pathlib import Path
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]


class IOSReadMemory(unittest.TestCase):
    def test_copy_results(self):
        patch = REPO / "patches/madeira-unix/0106-ntdll-return-a-read-error-for-inaccessible-self-memo.patch"
        added = "\n".join(line[1:] for line in patch.read_text().splitlines()
                          if line.startswith("+") and not line.startswith("+++"))
        code = "static NTSTATUS ios_read_virtual_memory(" + added.split(
            "static NTSTATUS ios_read_virtual_memory(", 1)[1].split("\n}", 1)[0] + "\n}\n"
        self.assertIn("status = ios_read_virtual_memory( addr, buffer, &size );", added)
        work = REPO / ".work"
        work.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="read-memory-", dir=work) as tmp:
            root = Path(tmp)
            source = root / "test.c"
            source.write_text(r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef int NTSTATUS;
typedef size_t SIZE_T, mach_vm_size_t;
typedef uintptr_t mach_vm_address_t;
#define STATUS_SUCCESS 0
#define STATUS_PARTIAL_COPY 1
#define KERN_SUCCESS 0
static int calls, result;
static size_t reported;
static const void *expected_source;
static void *expected_dest;
static size_t expected_size;
static int mach_task_self(void) { return 42; }
static int mach_vm_read_overwrite(int task, mach_vm_address_t src, mach_vm_size_t size,
                                  mach_vm_address_t dst, mach_vm_size_t *copied)
{
    ++calls;
    assert(task == 42 && src == (uintptr_t)expected_source);
    assert(dst == (uintptr_t)expected_dest && size == expected_size);
    *copied = reported;
    if (!result && reported == size) memmove((void *)dst, (const void *)src, size);
    return result;
}
''' + code + r'''
int main(void)
{
    unsigned char src[32] = "Tomb Raider", dst[32] = {0};
    size_t size = 0;
    /* Empty reads must not ask Mach to validate either pointer. */
    assert(ios_read_virtual_memory(NULL, NULL, &size) == STATUS_SUCCESS);
    assert(!calls && !size);
    expected_source = src; expected_dest = dst; expected_size = sizeof(src);
    size = reported = sizeof(src);
    assert(ios_read_virtual_memory(src, dst, &size) == STATUS_SUCCESS);
    assert(size == sizeof(src) && !memcmp(src, dst, size) && calls == 1);
    /* Unaligned addresses, as used by i386 callers, are passed through. */
    expected_source = src + 1; expected_dest = dst + 1;
    size = reported = expected_size = 7;
    assert(ios_read_virtual_memory(src + 1, dst + 1, &size) == STATUS_SUCCESS);
    /* Protection/unmap failures cannot become native loads. */
    expected_source = (void *)(uintptr_t)1;
    for (result = 1; result <= 3; ++result)
    {
        size = expected_size;
        reported = size; /* failure wins even if Mach left a full count */
        assert(ios_read_virtual_memory(expected_source, expected_dest, &size) == STATUS_PARTIAL_COPY);
        assert(!size);
    }
    result = 0; reported = 3; size = expected_size;
    assert(ios_read_virtual_memory(expected_source, expected_dest, &size) == STATUS_PARTIAL_COPY);
    assert(!size); /* short success keeps the old all-or-zero contract */
    return 0;
}
''')
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-fsanitize=undefined,address", str(source), "-o", str(root / "test")], check=True)
            subprocess.run([str(root / "test")], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()

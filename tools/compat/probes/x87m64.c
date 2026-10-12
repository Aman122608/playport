/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * x87m64: does a 64-bit integer come back exactly from the x87 stack?
 * (PLA-202.) 32-bit MSVC loads a 64-bit value atomically with
 * `fild qword [m]; fistp qword [m2]`; Mortal Kombat Komplete Edition keeps a
 * {pointer, tag} list head this way. FEX's X87ReducedPrecision holds the x87
 * stack in doubles (53 bits), so without patches/fex 0025 a value above 2^53
 * came back rounded (the pointer to 1 KiB under a tag with its top bit set).
 *
 *   exact    fild m64 then, with the value unchanged, fistp m64: directly,
 *            after fxch, from a copy (fld st0), and with fisttp; each must
 *            return the integer loaded, for fixed values above 2^53 and
 *            100,000 pseudo-random ones
 *   changed  fild m64, +0.0, fistp m64: printed, not checked; with a 64-bit
 *            significand (80-bit x87) the value is exact, through a double (or
 *            Windows' 53-bit precision control) it is rounded to 53 bits
 *
 * Each line goes to OutputDebugString, stdout and <dir>\x87m64.log; the last
 * line is "x87m64: done ok" (exit 0) or "x87m64: done FAIL" (1).
 * Built with llvm-mingw for i686 (x87m64-i386.exe) by
 * tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed;
static int fail_lines; /* the first 16 failures are printed */

static void probe_log(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "x87m64: ");

    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, line, (DWORD)strlen(line), &written, NULL);
}

/* Each sequence is one asm block, so it is one FEX block, as in the game. */
static long long fild_fistp(const long long *src)
{
    long long dst;
    __asm__ volatile("fildq %1\n\tfistpq %0" : "=m"(dst) : "m"(*src));
    return dst;
}

static long long fild_fxch_fistp(const long long *src)
{
    long long dst;
    __asm__ volatile("fildq %1\n\tfld1\n\tfxch %%st(1)\n\tfistpq %0\n\tfstp %%st(0)" : "=m"(dst) : "m"(*src));
    return dst;
}

static long long fild_copy_fistp(const long long *src)
{
    long long dst;
    __asm__ volatile("fildq %1\n\tfld %%st(0)\n\tfistpq %0\n\tfstp %%st(0)" : "=m"(dst) : "m"(*src));
    return dst;
}

static long long fild_fisttp(const long long *src)
{
    long long dst;
    __asm__ volatile("fildq %1\n\tfisttpq %0" : "=m"(dst) : "m"(*src));
    return dst;
}

static long long fild_add_zero_fistp(const long long *src)
{
    long long dst;
    __asm__ volatile("fildq %1\n\tfldz\n\tfaddp\n\tfistpq %0" : "=m"(dst) : "m"(*src));
    return dst;
}

static const struct
{
    const char *name;
    long long (*fn)(const long long *);
} exact_ops[] = {
    {"fild;fistp", fild_fistp},
    {"fild;fld1;fxch;fistp", fild_fxch_fistp},
    {"fild;fld st0;fistp", fild_copy_fistp},
    {"fild;fisttp", fild_fisttp},
};

static void check(unsigned long long value, int print)
{
    long long src = (long long)value;

    for (size_t i = 0; i < sizeof(exact_ops) / sizeof(exact_ops[0]); i++)
    {
        unsigned long long got = (unsigned long long)exact_ops[i].fn(&src);
        if (got != value)
        {
            failed = 1;
            if (fail_lines++ < 16) probe_log("CHECK exact %s 0x%016llx -> 0x%016llx FAIL\n", exact_ops[i].name, value, got);
        }
        else if (print)
            probe_log("CHECK exact %s 0x%016llx ok\n", exact_ops[i].name, value);
    }
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    static const unsigned long long fixed[] = {
        0x84280002320300a8ull, /* the game's {pointer, tag} order: tag 0x84280002 high */
        0x320300a884280002ull,
        0x0020000000000001ull, /* 2^53 + 1 */
        0x7fffffffffffffffull,
        0x8000000000000001ull,
        0xfffffffffffffffdull, /* -3 */
    };
    char dir[MAX_PATH], path[MAX_PATH + 16], *slash;
    unsigned long long x = 0x9e3779b97f4a7c15ull;
    int before;

    GetModuleFileNameA(NULL, dir, sizeof(dir));
    if ((slash = strrchr(dir, '\\'))) *slash = 0;
    snprintf(path, sizeof(path), "%s\\x87m64.log", dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    probe_log("start: %d-bit\n", (int)sizeof(void *) * 8);

    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) check(fixed[i], 1);

    before = fail_lines;
    for (int i = 0; i < 100000; i++)
    {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        check(x, 0);
    }
    probe_log("CHECK exact 100000 random values: %d failures %s\n", fail_lines - before, fail_lines > before ? "FAIL" : "ok");

    {
        long long src = (long long)fixed[0];
        unsigned long long got = (unsigned long long)fild_add_zero_fistp(&src);
        probe_log("changed fild;+0;fistp 0x%016llx -> 0x%016llx (%s)\n", fixed[0], got,
                  got == fixed[0] ? "exact: 64-bit significand" : "rounded: 53-bit significand");
    }

    probe_log("done %s\n", failed ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

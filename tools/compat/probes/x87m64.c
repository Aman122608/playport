/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * x87m64: does a 64-bit integer come back exactly from the x87 stack?
 * (PLA-202, PLA-235.) 32-bit MSVC loads a 64-bit value atomically with
 * `fild qword [m]; fistp qword [m2]`; Mortal Kombat Komplete Edition keeps a
 * {pointer, tag} list head this way. FEX's X87ReducedPrecision holds the x87
 * stack in doubles (53 bits), so without patches/fex 0025 a value above 2^53
 * came back rounded (the pointer to 1 KiB under a tag with its top bit set).
 *
 *   exact    fild m64 then, with the value unchanged, fistp m64: directly,
 *            after fxch, from a copy (fld st0), and with fisttp; each must
 *            return the integer loaded, for fixed values above 2^53 and
 *            100,000 pseudo-random ones
 *   blocks   the same with a branch between fild and fistp, so that they are
 *            in different FEX blocks (PLA-235): a conditional jump, and the
 *            two copies of Delphi's Move (the RTL's 8-byte copy by fild m64 and
 *            fistp m64): its small form (three loads, a jump, three stores in
 *            reverse) and its loop (the first 8 bytes loaded before the loop
 *            and stored after it); GOG's Inno Setup loader copies its path
 *            this way
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

/* Across FEX blocks: each branch target starts a new block, whose x87 stack is
 * read back from the thread's context (PLA-235). */
static long long fild_jcc_fistp(const long long *src)
{
    long long dst;
    __asm__ volatile("fildq %1\n\ttestl %%esp, %%esp\n\tjz 1f\n\tnop\n1:\n\tfistpq %0" : "=m"(dst) : "m"(*src) : "cc");
    return dst;
}

static long long fild_jcc_fisttp(const long long *src)
{
    long long dst;
    __asm__ volatile("fildq %1\n\ttestl %%esp, %%esp\n\tjz 1f\n\tnop\n1:\n\tfisttpq %0" : "=m"(dst) : "m"(*src) : "cc");
    return dst;
}

/* Delphi's Move for 9 to 32 bytes: last, first and second 8 bytes loaded, a
 * jump on the count, then stored in reverse order. Copies 24 bytes. */
static void delphi_small_move(const long long *src, long long *dst)
{
    __asm__ volatile("fildq 16(%0)\n\tfildq (%0)\n\tfildq 8(%0)\n\ttestl %%esp, %%esp\n\tjz 1f\n\tnop\n1:\n\t"
                     "fistpq 8(%1)\n\tfistpq (%1)\n\tfistpq 16(%1)"
                     :
                     : "r"(src), "r"(dst)
                     : "cc", "memory");
}

/* Delphi's Move for more than 32 bytes: the first 8 bytes loaded before the
 * loop, which copies the rest 8 bytes at a time, and stored after it. */
static void delphi_large_move(const long long *src, long long *dst, int count)
{
    __asm__ volatile("fildq (%0)\n\tmovl $1, %%ecx\n"
                     "1:\n\tfildq (%0,%%ecx,8)\n\tfistpq (%1,%%ecx,8)\n\tincl %%ecx\n\tcmpl %2, %%ecx\n\tjl 1b\n\t"
                     "fistpq (%1)"
                     :
                     : "r"(src), "r"(dst), "r"(count)
                     : "ecx", "cc", "memory");
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
    {"fild;jcc;fistp", fild_jcc_fistp},
    {"fild;jcc;fisttp", fild_jcc_fisttp},
};

static void check_moves(const unsigned long long *values, int print)
{
    long long src[6], dst[6];
    int same;

    memcpy(src, values, sizeof(src));
    memset(dst, 0, sizeof(dst));
    delphi_small_move(src, dst);
    same = memcmp(src, dst, 3 * sizeof(src[0])) == 0;
    if (!same)
    {
        failed = 1;
        if (fail_lines++ < 16)
            probe_log("CHECK blocks move-small 0x%016llx 0x%016llx 0x%016llx -> 0x%016llx 0x%016llx 0x%016llx FAIL\n", src[0], src[1],
                      src[2], dst[0], dst[1], dst[2]);
    }
    else if (print)
        probe_log("CHECK blocks move-small 0x%016llx 0x%016llx 0x%016llx ok\n", src[0], src[1], src[2]);

    memset(dst, 0, sizeof(dst));
    delphi_large_move(src, dst, 6);
    same = memcmp(src, dst, sizeof(src)) == 0;
    if (!same)
    {
        failed = 1;
        if (fail_lines++ < 16) probe_log("CHECK blocks move-large first 0x%016llx -> 0x%016llx FAIL\n", src[0], dst[0]);
    }
    else if (print)
        probe_log("CHECK blocks move-large first 0x%016llx ok\n", src[0]);
}

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
    {
        /* UTF-16 text as Inno's loader copies its path ("C:\G", "mes\", "N NW"), then fixed values. */
        static const unsigned long long path[6] = {0x0047005c003a0043ull, 0x005c00730065006dull, 0x0057004e0020004eull,
                                                   0x84280002320300a8ull, 0x0020000000000001ull, 0xfffffffffffffffdull};
        check_moves(path, 1);
    }

    before = fail_lines;
    for (int i = 0; i < 100000; i++)
    {
        unsigned long long values[6];

        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        check(x, 0);
        if (i % 6 == 0)
        {
            for (int j = 0; j < 6; j++)
            {
                x ^= x << 13;
                x ^= x >> 7;
                x ^= x << 17;
                values[j] = x;
            }
            check_moves(values, 0);
        }
    }
    probe_log("CHECK exact and blocks 100000 random values: %d failures %s\n", fail_lines - before, fail_lines > before ? "FAIL" : "ok");

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

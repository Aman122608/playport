/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * protect: what one-page NtProtectVirtualMemory calls cost, and the
 * user shared data read an anti-tamper layer makes (PLA-118). Football
 * Manager 2022's fm.exe changes single pages of its image to
 * PAGE_EXECUTE_READ, about 21,000 of them at start, and reads
 * KUSER_SHARED_DATA's interrupt time from x86 code.
 *
 *   usd     read InterruptTime (0x7ffe0008, its High1Time, LowPart and
 *           High2Time) as Windows' KSYSTEM_TIME loop does, 100,000 times; the
 *           value must not go back and must move across a 50 ms Sleep
 *   data    one page of a 1 MiB allocation, PAGE_READONLY then back to
 *           PAGE_READWRITE, 4,096 times each
 *   rx      the same with PAGE_EXECUTE_READ and PAGE_READWRITE
 *   image   the same on a 1 MiB section of the probe's own image
 *   exec    write `mov eax, N; ret` into a page, PAGE_EXECUTE_READ, call it,
 *           PAGE_READWRITE: 256 pages, 16 rounds; each call must return its N
 *
 * Each line goes to OutputDebugString, stdout and <dir>\protect.log; the
 * timings are microseconds per VirtualProtect call. The last line is
 * "protect: done ok" (exit 0) or "protect: done FAIL" (1).
 * Built with llvm-mingw for x86-64 (protect.exe) by
 * tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PAGE 4096
#define PAGES 256

static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed;
static LARGE_INTEGER freq;

/* 1 MiB of the image, page-aligned, flipped by the image phase. */
__attribute__((aligned(PAGE))) static unsigned char image_pages[PAGES * PAGE] = { 1 };

static void probe_log(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "protect: ");

    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, line, (DWORD)strlen(line), &written, NULL);
}

static double now_us(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1e6 / (double)freq.QuadPart;
}

/* KUSER_SHARED_DATA through a register, so each read is a load at an offset
 * from a base (as the game's is), not from an absolute address. */
static const unsigned char *volatile usd_base = (const unsigned char *)(ULONG_PTR)0x7ffe0000;

/* KSYSTEM_TIME at KUSER_SHARED_DATA+0x8: LowPart, High1Time, High2Time. */
static unsigned long long interrupt_time(void)
{
    volatile const unsigned char *usd = usd_base;
    LONG high1, high2;
    ULONG low;

    do
    {
        high1 = *(volatile const LONG *)(usd + 0x0c);
        low = *(volatile const ULONG *)(usd + 0x08);
        high2 = *(volatile const LONG *)(usd + 0x10);
    } while (high1 != high2);
    return ((unsigned long long)(ULONG)high1 << 32) | low;
}

static void usd_phase(void)
{
    unsigned long long first = interrupt_time(), last = first, t;
    int back = 0;
    double t0 = now_us();

    for (int i = 0; i < 100000; i++)
    {
        t = interrupt_time();
        if (t < last) back++;
        last = t;
    }
    probe_log("usd 100000 reads %.1f ns each, first %llu last %llu\n", (now_us() - t0) * 1000 / 100000, first, last);
    Sleep(50);
    t = interrupt_time();
    probe_log("CHECK usd interrupt time never back (%d) and moves across Sleep(50) (+%llu x 100 ns) %s\n",
              back, t - last, !back && t > last ? "ok" : "FAIL");
    if (back || t <= last) failed = 1;
}

/* One page at a time, `to` then `back`, 4,096 times each; returns us per call. */
static void flip_phase(const char *name, unsigned char *base, DWORD to, DWORD back)
{
    DWORD old;
    int bad = 0;
    double t0 = now_us(), us;

    for (int i = 0; i < 4096; i++)
    {
        unsigned char *page = base + (size_t)(i % PAGES) * PAGE;
        if (!VirtualProtect(page, PAGE, to, &old)) bad++;
        if (!VirtualProtect(page, PAGE, back, &old)) bad++;
    }
    us = (now_us() - t0) / 8192;
    probe_log("%s 8192 one-page VirtualProtect calls (0x%lx <-> 0x%lx): %.1f us each\n", name, to, back, us);
    probe_log("CHECK %s every call succeeded (%d failed) %s\n", name, bad, bad ? "FAIL" : "ok");
    if (bad) failed = 1;
}

static void exec_phase(void)
{
    unsigned char *base = VirtualAlloc(NULL, PAGES * PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD old;
    int wrong = 0, bad = 0;
    double t0 = now_us();

    if (!base)
    {
        probe_log("CHECK exec VirtualAlloc FAIL (%lu)\n", GetLastError());
        failed = 1;
        return;
    }
    for (int round = 0; round < 16; round++)
        for (int p = 0; p < PAGES; p++)
        {
            unsigned char *code = base + (size_t)p * PAGE;
            int (*fn)(void) = (int (*)(void))code;
            int want = round * PAGES + p;

            code[0] = 0xb8; /* mov eax, imm32 */
            memcpy(code + 1, &want, 4);
            code[5] = 0xc3; /* ret */
            if (!VirtualProtect(code, PAGE, PAGE_EXECUTE_READ, &old)) bad++;
            FlushInstructionCache(GetCurrentProcess(), code, 6);
            if (fn() != want) wrong++;
            if (!VirtualProtect(code, PAGE, PAGE_READWRITE, &old)) bad++;
        }
    probe_log("exec 4096 rounds of write, PAGE_EXECUTE_READ, call, PAGE_READWRITE: %.1f us each\n",
              (now_us() - t0) / 4096);
    probe_log("CHECK exec every call returned what was written (%d stale) and every protect succeeded (%d failed) %s\n",
              wrong, bad, wrong || bad ? "FAIL" : "ok");
    if (wrong || bad) failed = 1;
    VirtualFree(base, 0, MEM_RELEASE);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char dir[MAX_PATH], path[MAX_PATH + 16], *slash;
    unsigned char *data;

    QueryPerformanceFrequency(&freq);
    GetModuleFileNameA(NULL, dir, sizeof(dir));
    if ((slash = strrchr(dir, '\\'))) *slash = 0;
    snprintf(path, sizeof(path), "%s\\protect.log", dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    probe_log("start: %d-bit, image pages at %p\n", (int)sizeof(void *) * 8, (void *)image_pages);

    usd_phase();

    data = VirtualAlloc(NULL, PAGES * PAGE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!data)
    {
        probe_log("CHECK VirtualAlloc FAIL (%lu)\n", GetLastError());
        failed = 1;
    }
    else
    {
        memset(data, 0x5a, PAGES * PAGE);
        flip_phase("data", data, PAGE_READONLY, PAGE_READWRITE);
        flip_phase("rx", data, PAGE_EXECUTE_READ, PAGE_READWRITE);
        VirtualFree(data, 0, MEM_RELEASE);
    }
    flip_phase("image", image_pages, PAGE_EXECUTE_READ, PAGE_READWRITE);
    exec_phase();

    probe_log("done %s\n", failed ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

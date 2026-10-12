/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * splitlock: what a guest split lock costs and whether it stays correct
 * (PLA-242). A locked x86 operation on a slot that crosses a 16-byte or a
 * 64-byte boundary is a misaligned ARM64 atomic under FEX: the phone raises
 * an alignment fault for each one, which FEX (or the Mach exception handler,
 * for a CAS) completes. Jurassic World Evolution's protected code runs about
 * 14,000 of them a second.
 *
 * For each size (4 bytes; also 8 on x86-64) and each slot (aligned, inside
 * 16 bytes, across 16 bytes, across 64 bytes), single-threaded:
 *   xadd     `lock xadd` N times; each must return the running value
 *   cmpxchg  `lock cmpxchg` N times with the right expected value (each must
 *            succeed), then once with a wrong one (must fail, change nothing)
 *   xchg     `xchg` N times; each must return the value stored before
 * and the bytes around the slot must not change. Timings are ns per
 * operation. Then four threads contend on the 16-byte-crossing slot, with
 * `lock xadd`, a `lock cmpxchg` increment loop and an `xchg` spinlock around
 * a plain counter; lost updates there are reported as "torn" (FEX's handler
 * for an access across 16 bytes is two CAS loops and can tear, as AMD does)
 * and do not fail the probe.
 *
 * Each line goes to OutputDebugString, stdout and <dir>\splitlock.log. The
 * last line is "splitlock: done ok" (exit 0) or "splitlock: done FAIL" (1),
 * with the contended result after it.
 * Built with llvm-mingw for x86-64 (splitlock.exe) and i386
 * (splitlock-i386.exe) by tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define GUARD 0xa5
#define THREADS 4

static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed, torn;
static LARGE_INTEGER freq;

/* Two cache lines, 64-byte aligned; a slot is placed at an offset in it. */
static __attribute__((aligned(64))) unsigned char buf[128];

static void probe_log(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "splitlock: ");

    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, line, (DWORD)strlen(line), &written, NULL);
}

static double now_ns(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1e9 / (double)freq.QuadPart;
}

static uint32_t xadd32(volatile void *p, uint32_t v)
{
    __asm__ volatile("lock xaddl %0, %1" : "+r"(v), "+m"(*(volatile uint32_t *)p) : : "memory", "cc");
    return v;
}

static int cmpxchg32(volatile void *p, uint32_t *expected, uint32_t desired)
{
    unsigned char ok;
    uint32_t e = *expected;
    __asm__ volatile("lock cmpxchgl %3, %1\n\tsetz %2"
                     : "+a"(e), "+m"(*(volatile uint32_t *)p), "=q"(ok)
                     : "r"(desired)
                     : "memory", "cc");
    *expected = e;
    return ok;
}

static uint32_t xchg32(volatile void *p, uint32_t v)
{
    __asm__ volatile("xchgl %0, %1" : "+r"(v), "+m"(*(volatile uint32_t *)p) : : "memory");
    return v;
}

#ifdef __x86_64__
static uint64_t xadd64(volatile void *p, uint64_t v)
{
    __asm__ volatile("lock xaddq %0, %1" : "+r"(v), "+m"(*(volatile uint64_t *)p) : : "memory", "cc");
    return v;
}

static int cmpxchg64(volatile void *p, uint64_t *expected, uint64_t desired)
{
    unsigned char ok;
    uint64_t e = *expected;
    __asm__ volatile("lock cmpxchgq %3, %1\n\tsetz %2"
                     : "+a"(e), "+m"(*(volatile uint64_t *)p), "=q"(ok)
                     : "r"(desired)
                     : "memory", "cc");
    *expected = e;
    return ok;
}

static uint64_t xchg64(volatile void *p, uint64_t v)
{
    __asm__ volatile("xchgq %0, %1" : "+r"(v), "+m"(*(volatile uint64_t *)p) : : "memory");
    return v;
}
#endif

/* Unaligned plain access to the slot, outside the timed loops. */
static uint64_t slot_read(int off, int size)
{
    uint64_t v = 0;
    memcpy(&v, (const void *)(buf + off), size);
    return v;
}

static void slot_write(int off, int size, uint64_t v)
{
    memcpy((void *)(buf + off), &v, size);
}

static int guards_intact(int off, int size)
{
    for (int i = 0; i < (int)sizeof(buf); i++)
        if ((i < off || i >= off + size) && buf[i] != GUARD) return 0;
    return 1;
}

static uint64_t mask_of(int size)
{
    return size == 8 ? ~0ull : (1ull << (size * 8)) - 1;
}

/* A start value and a step whose sums carry across every byte, so a half of
 * the slot left behind shows. */
static const uint64_t start64 = 0x00f0f0f0f0f0f0f0ull, step64 = 0x0101010101010101ull;

static void xadd_phase(const char *where, int off, int size, int n)
{
    uint64_t m = mask_of(size), expect = start64 & m, step = step64 & m;
    volatile void *p = buf + off;
    int wrong = 0;
    double t0, t;

    memset(buf, GUARD, sizeof(buf));
    slot_write(off, size, expect);
    t0 = now_ns();
    for (int i = 0; i < n; i++)
    {
        uint64_t old;
#ifdef __x86_64__
        old = size == 8 ? xadd64(p, step) : xadd32(p, (uint32_t)step);
#else
        old = xadd32(p, (uint32_t)step);
#endif
        if (old != expect) wrong++;
        expect = (expect + step) & m;
    }
    t = (now_ns() - t0) / n;
    if (slot_read(off, size) != expect) wrong++;
    probe_log("xadd    %d-byte %-9s off %2d: %9.1f ns/op (%d ops)\n", size, where, off, t, n);
    if (wrong || !guards_intact(off, size))
    {
        probe_log("CHECK xadd %d-byte %s: %d wrong results, guards %s FAIL\n", size, where, wrong,
                  guards_intact(off, size) ? "intact" : "CHANGED");
        failed = 1;
    }
}

static void cmpxchg_phase(const char *where, int off, int size, int n)
{
    uint64_t m = mask_of(size), cur = start64 & m, step = step64 & m;
    volatile void *p = buf + off;
    int wrong = 0;
    double t0, t;

    memset(buf, GUARD, sizeof(buf));
    slot_write(off, size, cur);
    t0 = now_ns();
    for (int i = 0; i < n; i++)
    {
        uint64_t next = (cur + step) & m, seen = cur;
        int ok;
#ifdef __x86_64__
        if (size == 8)
            ok = cmpxchg64(p, &seen, next);
        else
#endif
        {
            uint32_t s = (uint32_t)seen;
            ok = cmpxchg32(p, &s, (uint32_t)next);
            seen = s;
        }
        if (!ok || seen != cur) wrong++;
        cur = next;
    }
    t = (now_ns() - t0) / n;
    {
        /* a wrong expected value: must fail, report the current one, store nothing */
        uint64_t seen = (cur ^ 1) & m;
        int ok;
#ifdef __x86_64__
        if (size == 8)
            ok = cmpxchg64(p, &seen, 0);
        else
#endif
        {
            uint32_t s = (uint32_t)seen;
            ok = cmpxchg32(p, &s, 0);
            seen = s;
        }
        if (ok || seen != cur) wrong++;
    }
    if (slot_read(off, size) != cur) wrong++;
    probe_log("cmpxchg %d-byte %-9s off %2d: %9.1f ns/op (%d ops)\n", size, where, off, t, n);
    if (wrong || !guards_intact(off, size))
    {
        probe_log("CHECK cmpxchg %d-byte %s: %d wrong results, guards %s FAIL\n", size, where, wrong,
                  guards_intact(off, size) ? "intact" : "CHANGED");
        failed = 1;
    }
}

static void xchg_phase(const char *where, int off, int size, int n)
{
    uint64_t m = mask_of(size), prev = start64 & m, step = step64 & m;
    volatile void *p = buf + off;
    int wrong = 0;
    double t0, t;

    memset(buf, GUARD, sizeof(buf));
    slot_write(off, size, prev);
    t0 = now_ns();
    for (int i = 0; i < n; i++)
    {
        uint64_t next = (prev + step) & m, old;
#ifdef __x86_64__
        old = size == 8 ? xchg64(p, next) : xchg32(p, (uint32_t)next);
#else
        old = xchg32(p, (uint32_t)next);
#endif
        if (old != prev) wrong++;
        prev = next;
    }
    t = (now_ns() - t0) / n;
    if (slot_read(off, size) != prev) wrong++;
    probe_log("xchg    %d-byte %-9s off %2d: %9.1f ns/op (%d ops)\n", size, where, off, t, n);
    if (wrong || !guards_intact(off, size))
    {
        probe_log("CHECK xchg %d-byte %s: %d wrong results, guards %s FAIL\n", size, where, wrong,
                  guards_intact(off, size) ? "intact" : "CHANGED");
        failed = 1;
    }
}

static void size_phases(int size)
{
    /* aligned and inside 16 bytes are the controls: no alignment fault there */
    static const struct { const char *where; int off4, off8, n; } slots[] = {
        { "aligned", 16, 16, 200000 },
        { "in-16", 17, 17, 200000 },
        { "across-16", 14, 12, 5000 },
        { "across-64", 62, 60, 5000 },
    };
    for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); i++)
    {
        int off = size == 8 ? slots[i].off8 : slots[i].off4;
        xadd_phase(slots[i].where, off, size, slots[i].n);
        cmpxchg_phase(slots[i].where, off, size, slots[i].n);
        xchg_phase(slots[i].where, off, size, slots[i].n);
    }
}

/* Contended: THREADS threads on the 4-byte slot across 16 bytes (offset 14). */
#define CONTEND_OFF 14
#define CONTEND_N 1000
static HANDLE go;
static volatile LONG plain_counter;
static unsigned char lock_buf_storage[128] __attribute__((aligned(64)));

static DWORD WINAPI xadd_worker(void *arg)
{
    WaitForSingleObject(go, INFINITE);
    for (int i = 0; i < CONTEND_N; i++) xadd32(buf + CONTEND_OFF, 1);
    return 0;
}

static DWORD WINAPI cmpxchg_worker(void *arg)
{
    WaitForSingleObject(go, INFINITE);
    for (int i = 0; i < CONTEND_N; i++)
    {
        uint32_t seen = (uint32_t)slot_read(CONTEND_OFF, 4);
        while (!cmpxchg32(buf + CONTEND_OFF, &seen, seen + 1)) { }
    }
    return 0;
}

static DWORD WINAPI xchg_worker(void *arg)
{
    volatile void *lock = lock_buf_storage + CONTEND_OFF;
    WaitForSingleObject(go, INFINITE);
    for (int i = 0; i < CONTEND_N; i++)
    {
        while (xchg32(lock, 1)) YieldProcessor();
        plain_counter = plain_counter + 1;
        xchg32(lock, 0);
    }
    return 0;
}

static void contend(const char *name, LPTHREAD_START_ROUTINE fn)
{
    HANDLE threads[THREADS];
    uint32_t got;
    double t0, t;

    memset(buf, 0, sizeof(buf));
    memset(lock_buf_storage, 0, sizeof(lock_buf_storage));
    plain_counter = 0;
    go = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < THREADS; i++) threads[i] = CreateThread(NULL, 0, fn, NULL, 0, NULL);
    Sleep(20);
    t0 = now_ns();
    SetEvent(go);
    WaitForMultipleObjects(THREADS, threads, TRUE, INFINITE);
    t = (now_ns() - t0) / (THREADS * CONTEND_N);
    for (int i = 0; i < THREADS; i++) CloseHandle(threads[i]);
    CloseHandle(go);
    got = fn == xchg_worker ? (uint32_t)plain_counter : (uint32_t)slot_read(CONTEND_OFF, 4);
    probe_log("CHECK-CONTENDED %s %d threads x %d across-16: %u of %u, %.1f ns/op %s\n", name, THREADS,
              CONTEND_N, got, THREADS * CONTEND_N, t, got == THREADS * CONTEND_N ? "ok" : "torn");
    if (got != THREADS * CONTEND_N) torn = 1;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char dir[MAX_PATH], path[MAX_PATH + 16], *slash;

    QueryPerformanceFrequency(&freq);
    GetModuleFileNameA(NULL, dir, sizeof(dir));
    if ((slash = strrchr(dir, '\\'))) *slash = 0;
    snprintf(path, sizeof(path), "%s\\splitlock.log", dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    probe_log("start: %d-bit, buffer at %p\n", (int)sizeof(void *) * 8, (void *)buf);

    size_phases(4);
#ifdef __x86_64__
    size_phases(8);
#endif
    contend("xadd", xadd_worker);
    contend("cmpxchg", cmpxchg_worker);
    contend("xchg-lock", xchg_worker);

    probe_log("done %s (contended: %s)\n", failed ? "FAIL" : "ok", torn ? "torn" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

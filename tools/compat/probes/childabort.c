/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * childabort: does a child process that fails, crashes or loses threads leave
 * the parent able to go on? (PLA-117, PLA-120.) Mortal Kombat Komplete Edition
 * (i386) runs `dxdiag.exe /whql:off /t FILE` while its own threads are busy;
 * on the phone dxdiag failed and the game's threads then spun for ever on the
 * FEX allocator's lock. This probe does the same and more, with worker threads
 * busy the whole time: each allocates and frees (malloc, HeapAlloc,
 * VirtualAlloc) and rewrites and calls a small piece of code, so FEX
 * translates, invalidates and allocates all the time.
 *
 *   dxdiag      run C:\windows\system32\dxdiag.exe /whql:off /t <dir>\dxdiag.txt
 *               as a child and wait for it (120 s at most); it must exit 0 and
 *               write the file (Wine's and Proton's dxdiag write only the
 *               System Information section: no "Dedicated Memory" line)
 *   crash       run this program as a child (`childabort.exe crash-child`) three
 *               times: the child starts busy workers and then writes to NULL
 *   terminate   (only when named) 30 rounds of starting four workers in this
 *               process that only rewrite and call code (no guest heap, whose
 *               lock a terminated thread would keep on Windows too) and ending
 *               them with TerminateThread once they work (each has counted at
 *               least once: on the phone a thread's start takes long enough
 *               that one ended in it kept the loader lock). A terminated worker
 *               must count no further 500 ms after its round (a CHECK): on the
 *               phone a terminated thread ran on until its next server call, and
 *               these workers make none, so all 120 ran on at full speed and
 *               starved the rest (PLA-120's evidence; PLA-200)
 *   rwx         (only when named) four threads that each, 1500 times, allocate an
 *               RWX page, write code into it, call it and free it, so a page one
 *               frees is often given at once to another. The code must run and
 *               return what was written: on the phone the first thread's free
 *               removed the second's executable range, whose first call then
 *               faulted as NOEXEC (PLA-228). Such a fault is counted and the
 *               call returns; the line also counts the pages reused at once
 *   noflush     (only when named) four workers that only rewrite and call code, as
 *               the others do but without FlushInstructionCache, for 3 s each in
 *               two layouts: the code in its own 4 KiB RWX allocation, and the code
 *               in the first 4 KiB page of a 16 KiB allocation whose other three
 *               pages are read-write data the worker writes on every round (one
 *               iOS host page). x86 needs no flush after a store to code, so every
 *               call must return the value just written: on the phone FEX trapped
 *               writes to code per 4 KiB guest page and an iOS host page is 16 KiB,
 *               so a store into a code page beside a writable one did not fault and
 *               the old translation ran (PLA-206). A CHECK per layout
 *   debugger    (only when named) run this program as a child
 *               (`childabort.exe debugger-child`) that writes to NULL with the
 *               default error mode, so kernelbase starts the AeDebug debugger
 *               (`winedbg --auto`); the child must end with c0000005 within 60 s,
 *               whether or not the debugger could attach (PLA-173)
 *   killwait    (only when named) 8 rounds of four threads parked in an
 *               in-process wait (two in RtlWaitOnAddress, two in
 *               SleepConditionVariableCS, both RtlWaitOnAddress in Wine) that
 *               are ended with TerminateThread; then every word is set and
 *               woken. A terminated waiter must not run on when woken (a
 *               CHECK): on the phone a killed thread parked there was never
 *               ended, and the wake let it go on (PLA-229). The waits and
 *               wakes that follow, on every futex bucket, must still work
 *   crashwait   (only when named) run this program as a child
 *               (`childabort.exe crashwait-child`) three times: two workers
 *               busy, two parked in RtlWaitOnAddress, then a write to NULL.
 *               Then the parent wakes an address in each of the 256 futex
 *               buckets, which walks whatever a dead child's thread left
 *               queued (on the phone the x86-64 ntdll copies of all
 *               pseudo-processes share the queue), and must live on (PLA-229,
 *               PLA-230)
 *
 * After each phase the parent checks that every worker it still runs made
 * progress within 15 s ("childabort: CHECK <phase> progress: ok|FAIL ..."); the
 * line also counts the calls of rewritten code that returned the value written
 * before (code changed in place without FEX translating it again).
 * Each line goes to OutputDebugString, stdout and <dir>\childabort.log; the
 * last line is "childabort: done ok" (exit 0) or "childabort: done FAIL" (1).
 *
 *    childabort.exe [PHASE...]     (default: dxdiag crash)
 *
 * Built with llvm-mingw for x86-64 (childabort.exe) and i686 (childabort-i386.exe) by
 * tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORKERS 4

static char dir[MAX_PATH];
static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed;

static void probe_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "childabort: ");

    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, line, (DWORD)strlen(line), &written, NULL);
}

/* One busy worker: allocation of every kind and code that changes under FEX. */
struct worker
{
    HANDLE thread;
    volatile LONG progress;
    volatile LONG stop;
    unsigned seed;
    int code_only;
    int no_flush;     /* noflush phase: rewrite code without FlushInstructionCache */
    int rw_neighbour; /* noflush phase: code page 0 of 16 KiB, pages 1-3 read-write data */
    volatile LONG stale_code;
};

/* The worker's code page: 4 KiB RWX, or page 0 of 16 KiB whose other pages are RW. */
static unsigned char *alloc_code(const struct worker *w)
{
    unsigned char *code;
    DWORD old;

    if (!w->rw_neighbour) return VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    code = VirtualAlloc(NULL, 16384, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (code && !VirtualProtect(code, 4096, PAGE_EXECUTE_READWRITE, &old))
    {
        VirtualFree(code, 0, MEM_RELEASE);
        return NULL;
    }
    return code;
}

static DWORD WINAPI busy_worker(void *arg)
{
    struct worker *w = arg;
    unsigned char *code = alloc_code(w);
    void *blocks[64] = {0};
    unsigned i = 0;

    while (!w->stop)
    {
        unsigned slot = (w->seed = w->seed * 1103515245 + 12345) % 64;
        size_t size = 16 + (w->seed >> 8) % 65536;

        if (w->code_only) goto code;
        free(blocks[slot]);
        blocks[slot] = malloc(size);
        if (blocks[slot]) memset(blocks[slot], (int)i, size < 256 ? size : 256);
        if (!(i % 16)) HeapFree(GetProcessHeap(), 0, HeapAlloc(GetProcessHeap(), 0, size * 4));
        if (!(i % 64))
        {
            void *big = VirtualAlloc(NULL, 1 << 20, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (big) { memset(big, 1, 4096); VirtualFree(big, 0, MEM_RELEASE); }
        }
    code:
        if (code && (w->code_only || !(i % 8)))
        {
            /* mov eax, imm32; ret: new code each time, so FEX translates it again;
             * and every 64th time a new page, so FEX also drops what it had */
            unsigned value = i;
            if (!(i % 64))
            {
                VirtualFree(code, 0, MEM_RELEASE);
                code = alloc_code(w);
                if (!code) break;
            }
            if (w->rw_neighbour) memcpy(code + 4096 + (i % 3) * 4096 + (i % 64) * 4, &value, 4);
            code[0] = 0xb8;
            memcpy(code + 1, &value, 4);
            code[5] = 0xc3;
            if (!w->no_flush) FlushInstructionCache(GetCurrentProcess(), code, 6);
            if (((unsigned (*)(void))code)() != value) InterlockedIncrement(&w->stale_code);
        }
        i++;
        InterlockedIncrement(&w->progress);
    }
    for (i = 0; i < 64; i++) free(blocks[i]);
    if (code) VirtualFree(code, 0, MEM_RELEASE);
    return 0;
}

static void start_workers(struct worker *w, int count, unsigned seed, int code_only)
{
    for (int i = 0; i < count; i++)
    {
        w[i].code_only = code_only;
        w[i].no_flush = 0;
        w[i].rw_neighbour = 0;
        w[i].stale_code = 0;
        w[i].progress = 0;
        w[i].stop = 0;
        w[i].seed = seed + i;
        w[i].thread = CreateThread(NULL, 0, busy_worker, &w[i], 0, NULL);
    }
}

static void stop_workers(struct worker *w, int count)
{
    for (int i = 0; i < count; i++) w[i].stop = 1;
    for (int i = 0; i < count; i++)
    {
        if (!w[i].thread) continue;
        if (WaitForSingleObject(w[i].thread, 15000) != WAIT_OBJECT_0)
        {
            probe_log("CHECK worker %d stops: FAIL (still running after 15 s)\n", i);
            failed = 1;
        }
        CloseHandle(w[i].thread);
        w[i].thread = NULL;
    }
}

/* Every worker counts up within 15 s. */
static void check_progress(const char *phase, struct worker *w, int count)
{
    LONG before[WORKERS];
    int stuck = 0;

    for (int i = 0; i < count; i++) before[i] = w[i].progress;
    for (int waited = 0; waited < 15000; waited += 250)
    {
        stuck = 0;
        Sleep(250);
        for (int i = 0; i < count; i++) if (w[i].progress == before[i]) stuck++;
        if (!stuck) break;
    }
    probe_log("CHECK %s progress: %s (%d of %d workers stuck; counts %ld %ld %ld %ld; "
              "rewritten code that returned the old value %ld %ld %ld %ld)\n", phase,
              stuck ? "FAIL" : "ok", stuck, count, w[0].progress, w[1].progress, w[2].progress,
              w[3].progress, w[0].stale_code, w[1].stale_code, w[2].stale_code, w[3].stale_code);
    if (stuck) failed = 1;
}

static DWORD run_child(const char *cmdline, DWORD timeout_ms)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    char buf[1024];
    DWORD code = (DWORD)-1, wait;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    snprintf(buf, sizeof(buf), "%s", cmdline);
    if (!CreateProcessA(NULL, buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        probe_log("CreateProcess(%s) failed: %lu\n", cmdline, GetLastError());
        return (DWORD)-2;
    }
    wait = WaitForSingleObject(pi.hProcess, timeout_ms);
    if (wait == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else probe_log("child still running after %lu s\n", timeout_ms / 1000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

static void phase_dxdiag(void)
{
    struct worker w[WORKERS];
    char out[MAX_PATH + 16], cmd[MAX_PATH + 96];
    HANDLE file;
    DWORD code, start;

    snprintf(out, sizeof(out), "%s\\dxdiag.txt", dir);
    DeleteFileA(out);
    snprintf(cmd, sizeof(cmd), "C:\\windows\\system32\\dxdiag.exe /whql:off /t %s", out);
    start_workers(w, WORKERS, 100, 0);
    start = GetTickCount();
    code = run_child(cmd, 120000);
    probe_log("dxdiag exit code %ld after %lu ms\n", (long)code, GetTickCount() - start);
    file = CreateFileA(out, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (code) failed = 1;
    if (file == INVALID_HANDLE_VALUE)
    {
        probe_log("CHECK dxdiag file: FAIL (%s not written)\n", out);
        failed = 1;
    }
    else
    {
        static char text[1 << 20];
        DWORD size = GetFileSize(file, NULL), got = 0;

        ReadFile(file, text, sizeof(text) - 1, &got, NULL);
        CloseHandle(file);
        text[got] = 0;
        /* dxdiag writes UTF-16 with a byte-order mark: keep the low bytes */
        if (got >= 2 && (unsigned char)text[0] == 0xff && (unsigned char)text[1] == 0xfe)
        {
            DWORD j = 0;
            for (DWORD k = 2; k + 1 < got; k += 2) text[j++] = text[k];
            text[j] = 0;
        }
        probe_log("CHECK dxdiag file: %s (%lu bytes%s)\n", strstr(text, "System Information") ? "ok" : "FAIL",
                  size, strstr(text, "Dedicated Memory") ? "; has Dedicated Memory" : "");
        if (!strstr(text, "System Information")) failed = 1;
    }
    check_progress("dxdiag", w, WORKERS);
    stop_workers(w, WORKERS);
}

static void phase_crash(const char *self)
{
    struct worker w[WORKERS];
    char cmd[MAX_PATH + 32];

    snprintf(cmd, sizeof(cmd), "\"%s\" crash-child", self);
    start_workers(w, WORKERS, 200, 0);
    for (int round = 0; round < 3; round++)
    {
        DWORD code = run_child(cmd, 60000);
        probe_log("crash child %d exit code 0x%lx\n", round, code);
    }
    check_progress("crash", w, WORKERS);
    stop_workers(w, WORKERS);
}

static void crash_child(int error_box)
{
    struct worker w[WORKERS];

    if (!error_box) SetErrorMode(SEM_NOGPFAULTERRORBOX);
    start_workers(w, WORKERS, 300, 0);
    Sleep(300);
    *(volatile int *)0 = 1;
}

static void phase_debugger(const char *self)
{
    char cmd[MAX_PATH + 32];
    DWORD code, start = GetTickCount();

    snprintf(cmd, sizeof(cmd), "\"%s\" debugger-child", self);
    code = run_child(cmd, 60000);
    probe_log("CHECK debugger child ends with c0000005: %s (exit code 0x%lx after %lu ms)\n",
              code == 0xc0000005 ? "ok" : "FAIL", code, GetTickCount() - start);
    if (code != 0xc0000005) failed = 1;
}

/* ntdll's RtlWaitOnAddress and RtlWakeAddressAll (Windows 8 and later) */
static NTSTATUS (WINAPI *pRtlWaitOnAddress)(const void *, const void *, SIZE_T, const LARGE_INTEGER *);
static void (WINAPI *pRtlWakeAddressAll)(const void *);

static int load_futex_calls(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");

    pRtlWaitOnAddress = (void *)GetProcAddress(ntdll, "RtlWaitOnAddress");
    pRtlWakeAddressAll = (void *)GetProcAddress(ntdll, "RtlWakeAddressAll");
    if (pRtlWaitOnAddress && pRtlWakeAddressAll) return 1;
    probe_log("CHECK futex calls: FAIL (ntdll has no RtlWaitOnAddress/RtlWakeAddressAll)\n");
    failed = 1;
    return 0;
}

/* One thread parked in an in-process wait until its word is set. */
struct waiter
{
    HANDLE thread;
    volatile LONG word;
    volatile LONG parked;
    volatile LONG ran_on;
    int condvar;
};

static CRITICAL_SECTION waiter_cs;
static CONDITION_VARIABLE waiter_cv = CONDITION_VARIABLE_INIT;

static DWORD WINAPI wait_worker(void *arg)
{
    struct waiter *w = arg;
    LONG zero = 0;

    if (w->condvar)
    {
        EnterCriticalSection(&waiter_cs);
        w->parked = 1;
        while (!w->word) SleepConditionVariableCS(&waiter_cv, &waiter_cs, INFINITE);
        LeaveCriticalSection(&waiter_cs);
    }
    else
    {
        w->parked = 1;
        while (!w->word) pRtlWaitOnAddress((const void *)&w->word, &zero, sizeof(zero), NULL);
    }
    InterlockedIncrement(&w->ran_on);
    return 0;
}

static void start_waiters(struct waiter *w, int count)
{
    for (int i = 0; i < count; i++)
    {
        w[i].word = 0;
        w[i].parked = 0;
        w[i].ran_on = 0;
        w[i].condvar = i & 1;
        w[i].thread = CreateThread(NULL, 0, wait_worker, &w[i], 0, NULL);
    }
    for (int waited = 0; waited < 5000; waited += 10)
    {
        int parked = 0;
        for (int i = 0; i < count; i++) if (w[i].parked) parked++;
        if (parked == count) break;
        Sleep(10);
    }
    Sleep(100); /* from parked to inside the wait */
}

/* Waits and wakes on an address in every futex bucket (16-byte steps): a
 * thread waits with a 1 ms timeout while the parent wakes the same address. */
static void futex_buckets(const char *phase)
{
    static volatile LONG words[256 * 4];
    LARGE_INTEGER timeout;
    LONG zero = 0;
    DWORD start = GetTickCount();

    timeout.QuadPart = -10000;
    for (int i = 0; i < 256; i++)
    {
        pRtlWakeAddressAll((const void *)&words[i * 4]);
        pRtlWaitOnAddress((const void *)&words[i * 4], &zero, sizeof(zero), &timeout);
    }
    probe_log("CHECK %s futex buckets: ok (256 woken and waited on in %lu ms)\n", phase,
              GetTickCount() - start);
}

#define KILLWAIT_ROUNDS 8

static void phase_killwait(void)
{
    static struct waiter waiters[KILLWAIT_ROUNDS][WORKERS];
    int ended = 0, ran_on = 0;
    DWORD start = GetTickCount();

    if (!load_futex_calls()) return;
    InitializeCriticalSection(&waiter_cs);
    for (int round = 0; round < KILLWAIT_ROUNDS; round++)
    {
        struct waiter *w = waiters[round];

        start_waiters(w, WORKERS);
        for (int i = 0; i < WORKERS; i++)
        {
            TerminateThread(w[i].thread, 0);
            if (WaitForSingleObject(w[i].thread, 15000) == WAIT_OBJECT_0) ended++;
            CloseHandle(w[i].thread);
            w[i].thread = NULL;
        }
    }
    Sleep(500);
    for (int round = 0; round < KILLWAIT_ROUNDS; round++)
        for (int i = 0; i < WORKERS; i++)
        {
            EnterCriticalSection(&waiter_cs);
            waiters[round][i].word = 1;
            LeaveCriticalSection(&waiter_cs);
            pRtlWakeAddressAll((const void *)&waiters[round][i].word);
        }
    WakeAllConditionVariable(&waiter_cv);
    Sleep(500);
    for (int round = 0; round < KILLWAIT_ROUNDS; round++)
        for (int i = 0; i < WORKERS; i++)
            if (waiters[round][i].ran_on) ran_on++;
    probe_log("CHECK killed waiters stay ended: %s (%d of %d ran on when woken; %d handles signalled; %lu ms)\n",
              ran_on ? "FAIL" : "ok", ran_on, KILLWAIT_ROUNDS * WORKERS, ended, GetTickCount() - start);
    if (ran_on || ended != KILLWAIT_ROUNDS * WORKERS) failed = 1;
    /* a killed waiter woken from SleepConditionVariableCS takes the critical
     * section back before its own code runs: one that then ended kept it */
    {
        int free_cs = 0;
        for (int waited = 0; waited < 2000 && !free_cs; waited += 10)
        {
            if ((free_cs = TryEnterCriticalSection(&waiter_cs))) LeaveCriticalSection(&waiter_cs);
            else Sleep(10);
        }
        probe_log("CHECK killwait critical section free: %s\n", free_cs ? "ok" : "FAIL (a killed waiter keeps it)");
        if (!free_cs)
        {
            failed = 1;
            return;
        }
    }
    futex_buckets("killwait");
    {
        /* a live waiter still parks and wakes */
        struct waiter live[2];
        start_waiters(live, 2);
        for (int i = 0; i < 2; i++)
        {
            EnterCriticalSection(&waiter_cs);
            live[i].word = 1;
            LeaveCriticalSection(&waiter_cs);
            pRtlWakeAddressAll((const void *)&live[i].word);
        }
        WakeAllConditionVariable(&waiter_cv);
        for (int i = 0; i < 2; i++)
        {
            DWORD wait = WaitForSingleObject(live[i].thread, 5000);
            probe_log("CHECK killwait live waiter %d wakes: %s\n", i, wait == WAIT_OBJECT_0 ? "ok" : "FAIL");
            if (wait != WAIT_OBJECT_0) failed = 1;
            CloseHandle(live[i].thread);
        }
    }
}

static void phase_crashwait(const char *self)
{
    struct worker w[WORKERS];
    char cmd[MAX_PATH + 32];

    if (!load_futex_calls()) return;
    snprintf(cmd, sizeof(cmd), "\"%s\" crashwait-child", self);
    start_workers(w, WORKERS, 700, 0);
    for (int round = 0; round < 3; round++)
    {
        DWORD code = run_child(cmd, 60000);
        probe_log("crashwait child %d exit code 0x%lx\n", round, code);
        if (code != 0xc0000005) failed = 1;
        futex_buckets("crashwait");
    }
    check_progress("crashwait", w, WORKERS);
    stop_workers(w, WORKERS);
}

/* Two busy workers and two parked in RtlWaitOnAddress, then a crash. */
static void crashwait_child(void)
{
    static struct waiter waiters[2];
    struct worker w[2];

    SetErrorMode(SEM_NOGPFAULTERRORBOX);
    if (!load_futex_calls()) ExitProcess(2);
    InitializeCriticalSection(&waiter_cs);
    waiters[0].condvar = waiters[1].condvar = 0;
    for (int i = 0; i < 2; i++)
    {
        waiters[i].word = waiters[i].parked = 0;
        waiters[i].thread = CreateThread(NULL, 0, wait_worker, &waiters[i], 0, NULL);
    }
    start_workers(w, 2, 800, 0);
    Sleep(300);
    *(volatile int *)0 = 1;
}

#define TERMINATE_ROUNDS 30

static void phase_terminate(void)
{
    static struct worker victims[TERMINATE_ROUNDS][WORKERS];
    struct worker keep[WORKERS];
    LONG ended_at[TERMINATE_ROUNDS][WORKERS];
    int ran_on = 0;
    DWORD start = GetTickCount();

    start_workers(keep, WORKERS, 400, 0);
    for (int round = 0; round < TERMINATE_ROUNDS; round++)
    {
        struct worker *v = victims[round];

        start_workers(v, WORKERS, 500 + round * WORKERS, 1);
        /* end them while they work, not while they start: a thread ended in
         * its start keeps what it holds then (the loader lock) on Windows too */
        for (int waited = 0; waited < 5000; waited += 10)
        {
            int started = 0;
            for (int i = 0; i < WORKERS; i++) if (v[i].progress) started++;
            if (started == WORKERS) break;
            Sleep(10);
        }
        Sleep(20 + round % 7 * 10);
        for (int i = 0; i < WORKERS; i++)
        {
            if (!v[i].thread) continue;
            TerminateThread(v[i].thread, 0);
            WaitForSingleObject(v[i].thread, 15000);
            CloseHandle(v[i].thread);
            v[i].thread = NULL;
        }
        Sleep(500);
        for (int i = 0; i < WORKERS; i++) ended_at[round][i] = v[i].progress;
        if (round % 10 == 9) probe_log("terminate: %d rounds after %lu ms\n", round + 1, GetTickCount() - start);
    }
    probe_log("terminate: %d rounds of %d workers ended in %lu ms\n", TERMINATE_ROUNDS, WORKERS,
              GetTickCount() - start);
    Sleep(500);
    for (int round = 0; round < TERMINATE_ROUNDS; round++)
        for (int i = 0; i < WORKERS; i++)
            if (victims[round][i].progress != ended_at[round][i]) ran_on++;
    probe_log("CHECK terminated workers stop: %s (%d of %d counted on after they were terminated)\n",
              ran_on ? "FAIL" : "ok", ran_on, TERMINATE_ROUNDS * WORKERS);
    if (ran_on) failed = 1;
    check_progress("terminate", keep, WORKERS);
    stop_workers(keep, WORKERS);
}

/* Rewritten code without a flush must still run as written, in both layouts. */
static void phase_noflush(void)
{
    struct worker w[WORKERS];

    for (int layout = 0; layout < 2; layout++)
    {
        const char *name = layout ? "noflush rw-neighbour" : "noflush 4k";
        LONG calls = 0, stale = 0;

        for (int i = 0; i < WORKERS; i++)
        {
            w[i].code_only = 1;
            w[i].no_flush = 1;
            w[i].rw_neighbour = layout;
            w[i].stale_code = 0;
            w[i].progress = 0;
            w[i].stop = 0;
            w[i].seed = 600 + layout * WORKERS + i;
            w[i].thread = CreateThread(NULL, 0, busy_worker, &w[i], 0, NULL);
        }
        Sleep(3000);
        check_progress(name, w, WORKERS);
        stop_workers(w, WORKERS);
        for (int i = 0; i < WORKERS; i++)
        {
            calls += w[i].progress;
            stale += w[i].stale_code;
        }
        probe_log("CHECK %s rewritten code runs: %s (%ld of %ld calls returned the old value)\n", name,
                  stale ? "FAIL" : "ok", stale, calls);
        if (stale) failed = 1;
    }
}

#define RWX_THREADS 4
#define RWX_PAGES 1500

static unsigned char *volatile rwx_page[RWX_THREADS], *volatile rwx_freed[RWX_THREADS];
static volatile LONG rwx_faults, rwx_wrong, rwx_reused, rwx_done[RWX_THREADS];

/* A call into a thread's own page that faults on execute returns 0xffffffff. */
static LONG CALLBACK rwx_fault_handler(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *rec = ep->ExceptionRecord;
    CONTEXT *ctx = ep->ContextRecord;

    if (rec->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || rec->NumberParameters < 2 ||
        rec->ExceptionInformation[0] != 8) return EXCEPTION_CONTINUE_SEARCH;
    for (int i = 0; i < RWX_THREADS; i++)
        if (rwx_page[i] && rec->ExceptionInformation[1] == (ULONG_PTR)rwx_page[i])
        {
            InterlockedIncrement(&rwx_faults);
#ifdef _WIN64
            ctx->Rip = *(DWORD64 *)ctx->Rsp;
            ctx->Rsp += 8;
            ctx->Rax = 0xffffffff;
#else
            ctx->Eip = *(DWORD *)ctx->Esp;
            ctx->Esp += 4;
            ctx->Eax = 0xffffffff;
#endif
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI rwx_worker(void *arg)
{
    int self = (int)(INT_PTR)arg;

    for (unsigned i = 0; i < RWX_PAGES; i++)
    {
        unsigned value = self << 24 | i;
        unsigned char *code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

        if (!code) break;
        for (int j = 0; j < RWX_THREADS; j++)
            if (j != self && rwx_freed[j] == code) InterlockedIncrement(&rwx_reused);
        rwx_page[self] = code;
        code[0] = 0xb8; /* mov eax, imm32; ret */
        memcpy(code + 1, &value, 4);
        code[5] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), code, 6);
        if (((unsigned (*)(void))code)() != value) InterlockedIncrement(&rwx_wrong);
        rwx_page[self] = NULL;
        VirtualFree(code, 0, MEM_RELEASE);
        rwx_freed[self] = code;
        rwx_done[self] = i + 1;
    }
    return 0;
}

static void phase_rwx(void)
{
    HANDLE threads[RWX_THREADS];
    void *handler = AddVectoredExceptionHandler(1, rwx_fault_handler);
    DWORD start = GetTickCount();

    for (int i = 0; i < RWX_THREADS; i++) threads[i] = CreateThread(NULL, 0, rwx_worker, (void *)(INT_PTR)i, 0, NULL);
    WaitForMultipleObjects(RWX_THREADS, threads, TRUE, 120000);
    for (int i = 0; i < RWX_THREADS; i++) CloseHandle(threads[i]);
    RemoveVectoredExceptionHandler(handler);
    probe_log("CHECK rwx reuse: %s (%ld calls faulted, %ld returned another value; %ld pages reused at "
              "once; pages per thread %ld %ld %ld %ld of %d; %lu ms)\n",
              rwx_faults || rwx_wrong ? "FAIL" : "ok", rwx_faults, rwx_wrong, rwx_reused, rwx_done[0],
              rwx_done[1], rwx_done[2], rwx_done[3], RWX_PAGES, GetTickCount() - start);
    if (rwx_faults || rwx_wrong) failed = 1;
    for (int i = 0; i < RWX_THREADS; i++) if (rwx_done[i] != RWX_PAGES) failed = 1;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char self[MAX_PATH], *slash;
    int argc = __argc;
    char **argv = __argv;

    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(dir, sizeof(dir), "%s", self);
    if ((slash = strrchr(dir, '\\'))) *slash = 0;
    if (argc > 1 && (!strcmp(argv[1], "crash-child") || !strcmp(argv[1], "debugger-child")))
    {
        crash_child(!strcmp(argv[1], "debugger-child"));
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "crashwait-child"))
    {
        crashwait_child();
        return 0;
    }
    {
        char path[MAX_PATH + 16];
        snprintf(path, sizeof(path), "%s\\childabort.log", dir);
        log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    }
    probe_log("start: %s, %d-bit\n", self, (int)sizeof(void *) * 8);
    for (int i = 1; i < argc || i == 1; i++)
    {
        const char *phase = argc > 1 ? argv[i] : NULL;
        if (!phase || !strcmp(phase, "dxdiag")) phase_dxdiag();
        if (!phase || !strcmp(phase, "crash")) phase_crash(self);
        if (phase && !strcmp(phase, "terminate")) phase_terminate();
        if (phase && !strcmp(phase, "debugger")) phase_debugger(self);
        if (phase && !strcmp(phase, "rwx")) phase_rwx();
        if (phase && !strcmp(phase, "noflush")) phase_noflush();
        if (phase && !strcmp(phase, "killwait")) phase_killwait();
        if (phase && !strcmp(phase, "crashwait")) phase_crashwait(self);
        if (!phase) break;
    }
    probe_log("done %s\n", failed ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

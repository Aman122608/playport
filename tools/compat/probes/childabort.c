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
 *   debugger    (only when named) run this program as a child
 *               (`childabort.exe debugger-child`) that writes to NULL with the
 *               default error mode, so kernelbase starts the AeDebug debugger
 *               (`winedbg --auto`); the child must end with c0000005 within 60 s,
 *               whether or not the debugger could attach (PLA-173)
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
 * Built with llvm-mingw for i686 (childabort-i386.exe) by
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
    volatile LONG stale_code;
};

static DWORD WINAPI busy_worker(void *arg)
{
    struct worker *w = arg;
    unsigned char *code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
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
                code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                if (!code) break;
            }
            code[0] = 0xb8;
            memcpy(code + 1, &value, 4);
            code[5] = 0xc3;
            FlushInstructionCache(GetCurrentProcess(), code, 6);
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
        if (!phase) break;
    }
    probe_log("done %s\n", failed ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

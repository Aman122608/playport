/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * threadctx: what GetThreadContext and SetThreadContext report for another
 * guest thread (PLA-245). Protections, Mono and Unity's GC and crash reporters
 * suspend a thread and read its x86 context; Jurassic World Evolution's
 * protection reads one of a thread it has just made and calls into zeros when
 * the answer is wrong.
 *
 *   a  a thread created suspended, not yet run: rip/eip must be
 *      RtlUserThreadStart in ntdll, with the start address and its parameter
 *      in rcx/rdx (eax/ebx on i386), rsp/esp on the thread's own stack
 *   b  a thread spinning in a guest loop: after SuspendThread, rip/eip must
 *      be inside the loop, rsp/esp on its stack and rax/eax the loop's marker
 *   c  a thread blocked in WaitForSingleObject and one in Sleep: rip/eip must
 *      be in a loaded image (ntdll on Windows and desktop Wine), rsp/esp on
 *      the thread's stack; the thread must then still wake and end
 *   d  SetThreadContext: a never-run thread pointed at another start routine
 *      and parameter (rcx/rdx, eax/ebx) must run that routine with that
 *      parameter; rax/eax set on the spinning thread must read back
 *
 * Each line goes to OutputDebugString, stdout and <dir>\threadctx.log. A line
 * ends "ok" or "BAD"; the last is "threadctx: done ok" (exit 0) or
 * "threadctx: done FAIL" (1). Built with llvm-mingw for x86-64
 * (threadctx.exe) and i386 (threadctx-i386.exe) by tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <winternl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __x86_64__
#define REG_IP(c) ((uintptr_t)(c)->Rip)
#define REG_SP(c) ((uintptr_t)(c)->Rsp)
#define REG_A0(c) ((uintptr_t)(c)->Rcx) /* start address */
#define REG_A1(c) ((uintptr_t)(c)->Rdx) /* its parameter */
#define REG_AX(c) ((uintptr_t)(c)->Rax)
#define SET_A0(c, v) ((c)->Rcx = (v))
#define SET_A1(c, v) ((c)->Rdx = (v))
#define SET_AX(c, v) ((c)->Rax = (v))
#define NAME_IP "rip"
#define NAME_SP "rsp"
#define NAME_A0 "rcx"
#define NAME_A1 "rdx"
#define NAME_AX "rax"
#define LOOP_MARK ((uintptr_t)0x5eed1234cafe5678ull)
#define LOOP_MARK2 ((uintptr_t)0x0badf00d87654321ull)
#define SYM(x) #x
#else
#define REG_IP(c) ((uintptr_t)(c)->Eip)
#define REG_SP(c) ((uintptr_t)(c)->Esp)
#define REG_A0(c) ((uintptr_t)(c)->Eax)
#define REG_A1(c) ((uintptr_t)(c)->Ebx)
#define REG_AX(c) ((uintptr_t)(c)->Eax)
#define SET_A0(c, v) ((c)->Eax = (v))
#define SET_A1(c, v) ((c)->Ebx = (v))
#define SET_AX(c, v) ((c)->Eax = (v))
#define NAME_IP "eip"
#define NAME_SP "esp"
#define NAME_A0 "eax"
#define NAME_A1 "ebx"
#define NAME_AX "eax"
#define LOOP_MARK ((uintptr_t)0x5eed1234u)
#define LOOP_MARK2 ((uintptr_t)0x87654321u)
#define SYM(x) "_" #x
#endif

#define PARAM_A ((uintptr_t)0x1a2b3c4d)
#define PARAM_B ((uintptr_t)0x2b3c4d5e)
#define SAMPLES 16

static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed;

static void probe_log(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "threadctx: ");

    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, line, (DWORD)strlen(line), &written, NULL);
}

static const char *verdict(int ok)
{
    if (!ok) failed = 1;
    return ok ? "ok" : "BAD";
}

/* The guest loop of phase b: the marker in rax/eax, then spin on loop_stop.
 * busy_loop_end bounds it. */
volatile LONG loop_stop;
void busy_loop(void);
extern const char busy_loop_end[];
#ifdef __x86_64__
__asm__(".text\n"
        ".globl " SYM(busy_loop) "\n" SYM(busy_loop) ":\n"
        "\tmovabsq $0x5eed1234cafe5678, %rax\n"
        "1:\tmovl " SYM(loop_stop) "(%rip), %ecx\n"
        "\ttestl %ecx, %ecx\n"
        "\tjz 1b\n"
        "\tret\n"
        ".globl " SYM(busy_loop_end) "\n" SYM(busy_loop_end) ":\n"
        "\tnop\n");
#else
__asm__(".text\n"
        ".globl " SYM(busy_loop) "\n" SYM(busy_loop) ":\n"
        "\tmovl $0x5eed1234, %eax\n"
        "1:\tmovl " SYM(loop_stop) ", %ecx\n"
        "\ttestl %ecx, %ecx\n"
        "\tjz 1b\n"
        "\tret\n"
        ".globl " SYM(busy_loop_end) "\n" SYM(busy_loop_end) ":\n"
        "\tnop\n");
#endif

static volatile uintptr_t ran_a, ran_b;

static DWORD WINAPI start_a(void *arg)
{
    ran_a = (uintptr_t)arg;
    return 0;
}

static DWORD WINAPI start_b(void *arg)
{
    ran_b = (uintptr_t)arg;
    return 0;
}

static DWORD WINAPI spin_thread(void *arg)
{
    busy_loop();
    return 0;
}

static HANDLE wake_event;

static DWORD WINAPI wait_thread(void *arg)
{
    return WaitForSingleObject(wake_event, INFINITE);
}

static DWORD WINAPI sleep_thread(void *arg)
{
    Sleep(1500);
    return 0;
}

static const char *module_of(uintptr_t addr, char *buf, DWORD size)
{
    HMODULE mod;
    char *slash;

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const char *)addr, &mod) ||
        !GetModuleFileNameA(mod, buf, size))
        return NULL;
    if ((slash = strrchr(buf, '\\'))) return slash + 1;
    return buf;
}

/* rsp/esp is on the thread's stack: the same allocation as just below the
 * TEB's StackBase. */
static int on_stack(HANDLE thread, uintptr_t sp)
{
    static NTSTATUS (WINAPI *query)(HANDLE, THREADINFOCLASS, void *, ULONG, ULONG *);
    struct { NTSTATUS exit_status; NT_TIB *teb; void *client_id[2]; ULONG_PTR affinity; LONG priority, base; } info;
    MEMORY_BASIC_INFORMATION a, b;
    NT_TIB *tib;

    if (!query) query = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");
    if (!sp || !query || query(thread, ThreadBasicInformation, &info, sizeof(info), NULL)) return 0;
    tib = info.teb;
    if (!VirtualQuery((void *)((uintptr_t)tib->StackBase - 1), &a, sizeof(a)) ||
        !VirtualQuery((void *)sp, &b, sizeof(b)))
        return 0;
    return a.AllocationBase == b.AllocationBase && sp <= (uintptr_t)tib->StackBase;
}

static int get_context(HANDLE thread, CONTEXT *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ContextFlags = CONTEXT_FULL;
    return GetThreadContext(thread, ctx);
}

static int wait_end(HANDLE thread, const char *what)
{
    DWORD r = WaitForSingleObject(thread, 5000);
    if (r != WAIT_OBJECT_0)
    {
        probe_log("CHECK %s: the thread did not end within 5 s (wait %lu) BAD\n", what, r);
        failed = 1;
        TerminateThread(thread, 1);
        return 0;
    }
    return 1;
}

/* a and d1: a never-run thread. */
static void phase_unstarted(void)
{
    uintptr_t start = (uintptr_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlUserThreadStart");
    char name[MAX_PATH];
    const char *mod;
    CONTEXT ctx;
    HANDLE t;
    int ok;

    ran_a = ran_b = 0;
    t = CreateThread(NULL, 0, start_a, (void *)PARAM_A, CREATE_SUSPENDED, NULL);
    if (!get_context(t, &ctx))
    {
        probe_log("CHECK a-unstarted: GetThreadContext failed (%lu) BAD\n", GetLastError());
        failed = 1;
        ResumeThread(t);
        wait_end(t, "a-unstarted");
        CloseHandle(t);
        return;
    }
    mod = module_of(REG_IP(&ctx), name, sizeof(name));
    probe_log("a-unstarted: " NAME_IP "=%p (%s; RtlUserThreadStart %p) " NAME_SP "=%p " NAME_A0 "=%p " NAME_A1
              "=%p flags=%08lx\n",
              (void *)REG_IP(&ctx), mod ? mod : "no image", (void *)start, (void *)REG_SP(&ctx),
              (void *)REG_A0(&ctx), (void *)REG_A1(&ctx), ctx.ContextFlags);
    ok = REG_IP(&ctx) == start;
    probe_log("CHECK a-unstarted " NAME_IP " is RtlUserThreadStart: %s\n", verdict(ok));
    if (!ok)
        probe_log("CHECK a-unstarted " NAME_IP " in ntdll: %s\n", verdict(mod && !_stricmp(mod, "ntdll.dll")));
    probe_log("CHECK a-unstarted " NAME_A0 " is the start address: %s\n",
              verdict(REG_A0(&ctx) == (uintptr_t)start_a));
    probe_log("CHECK a-unstarted " NAME_A1 " is the parameter: %s\n", verdict(REG_A1(&ctx) == PARAM_A));
    probe_log("CHECK a-unstarted " NAME_SP " on its stack: %s\n", verdict(on_stack(t, REG_SP(&ctx))));

    /* d1: point it at start_b with PARAM_B, read it back, run it. */
    SET_A0(&ctx, (uintptr_t)start_b);
    SET_A1(&ctx, PARAM_B);
    ok = SetThreadContext(t, &ctx);
    probe_log("CHECK d-set-unstarted SetThreadContext: %s\n", verdict(ok));
    get_context(t, &ctx);
    probe_log("CHECK d-set-unstarted reads back " NAME_A0 "=%p " NAME_A1 "=%p: %s\n", (void *)REG_A0(&ctx),
              (void *)REG_A1(&ctx), verdict(REG_A0(&ctx) == (uintptr_t)start_b && REG_A1(&ctx) == PARAM_B));
    ResumeThread(t);
    if (wait_end(t, "d-set-unstarted"))
        probe_log("CHECK d-set-unstarted runs start_b(%p) (start_a %p): %s\n", (void *)ran_b, (void *)ran_a,
                  verdict(ran_b == PARAM_B && !ran_a));
    CloseHandle(t);
}

/* b and d2: a thread in a guest loop. */
static void phase_spin(void)
{
    uintptr_t lo = (uintptr_t)busy_loop, hi = (uintptr_t)busy_loop_end;
    int in_loop = 0, on_stk = 0, marked = 0, got = 0;
    char name[MAX_PATH];
    const char *mod;
    CONTEXT ctx;
    HANDLE t;

    loop_stop = 0;
    t = CreateThread(NULL, 0, spin_thread, NULL, 0, NULL);
    Sleep(100);
    for (int i = 0; i < SAMPLES; i++)
    {
        if (SuspendThread(t) == (DWORD)-1) break;
        if (get_context(t, &ctx))
        {
            got++;
            in_loop += REG_IP(&ctx) >= lo && REG_IP(&ctx) < hi;
            on_stk += on_stack(t, REG_SP(&ctx));
            marked += REG_AX(&ctx) == LOOP_MARK;
            if (i < 3 || (i == SAMPLES - 1))
            {
                mod = module_of(REG_IP(&ctx), name, sizeof(name));
                probe_log("b-spin #%d: " NAME_IP "=%p (%s; loop %p-%p) " NAME_SP "=%p " NAME_AX "=%p\n", i,
                          (void *)REG_IP(&ctx), mod ? mod : "no image", (void *)lo, (void *)hi,
                          (void *)REG_SP(&ctx), (void *)REG_AX(&ctx));
            }
        }
        ResumeThread(t);
        Sleep(5);
    }
    probe_log("CHECK b-spin GetThreadContext succeeded %d of %d: %s\n", got, SAMPLES, verdict(got == SAMPLES));
    probe_log("CHECK b-spin " NAME_IP " in the loop %d of %d: %s\n", in_loop, got, verdict(got && in_loop == got));
    probe_log("CHECK b-spin " NAME_SP " on its stack %d of %d: %s\n", on_stk, got, verdict(got && on_stk == got));
    probe_log("CHECK b-spin " NAME_AX " is the marker %d of %d: %s\n", marked, got, verdict(got && marked == got));

    /* d2: set rax/eax while it spins, read it back after it ran on. */
    if (SuspendThread(t) != (DWORD)-1 && get_context(t, &ctx))
    {
        int ok;
        SET_AX(&ctx, LOOP_MARK2);
        ok = SetThreadContext(t, &ctx);
        probe_log("CHECK d-set-spin SetThreadContext: %s\n", verdict(ok));
        ResumeThread(t);
        Sleep(20);
        SuspendThread(t);
        get_context(t, &ctx);
        probe_log("CHECK d-set-spin " NAME_AX " reads back %p: %s\n", (void *)REG_AX(&ctx),
                  verdict(REG_AX(&ctx) == LOOP_MARK2));
        probe_log("d-set-spin " NAME_IP "=%p (loop %p-%p)\n", (void *)REG_IP(&ctx), (void *)lo, (void *)hi);
        ResumeThread(t);
    }
    loop_stop = 1;
    wait_end(t, "b-spin");
    CloseHandle(t);
}

/* c: a thread in a wait (native code). */
static void phase_blocked(const char *what, LPTHREAD_START_ROUTINE fn)
{
    int in_image = 0, on_stk = 0, got = 0;
    char name[MAX_PATH], first[MAX_PATH] = "";
    const char *mod;
    CONTEXT ctx;
    HANDLE t;

    wake_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    t = CreateThread(NULL, 0, fn, NULL, 0, NULL);
    Sleep(200);
    for (int i = 0; i < 4; i++)
    {
        if (SuspendThread(t) == (DWORD)-1) break;
        if (get_context(t, &ctx))
        {
            got++;
            mod = module_of(REG_IP(&ctx), name, sizeof(name));
            in_image += mod != NULL;
            on_stk += on_stack(t, REG_SP(&ctx));
            if (!i) snprintf(first, sizeof(first), "%s", mod ? mod : "no image");
            if (!i)
                probe_log("c-%s: " NAME_IP "=%p (%s) " NAME_SP "=%p\n", what, (void *)REG_IP(&ctx),
                          mod ? mod : "no image", (void *)REG_SP(&ctx));
        }
        ResumeThread(t);
        Sleep(20);
    }
    probe_log("CHECK c-%s GetThreadContext succeeded %d of 4: %s\n", what, got, verdict(got == 4));
    probe_log("CHECK c-%s " NAME_IP " in an image (%s) %d of %d: %s\n", what, first, in_image, got,
              verdict(got && in_image == got));
    probe_log("CHECK c-%s " NAME_SP " on its stack %d of %d: %s\n", what, on_stk, got, verdict(got && on_stk == got));
    SetEvent(wake_event);
    if (wait_end(t, what)) probe_log("CHECK c-%s wakes and ends: ok\n", what);
    CloseHandle(t);
    CloseHandle(wake_event);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char dir[MAX_PATH], path[MAX_PATH + 16], *slash;

    GetModuleFileNameA(NULL, dir, sizeof(dir));
    if ((slash = strrchr(dir, '\\'))) *slash = 0;
    snprintf(path, sizeof(path), "%s\\threadctx.log", dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    probe_log("start: %d-bit, start_a %p start_b %p busy_loop %p\n", (int)sizeof(void *) * 8, (void *)start_a,
              (void *)start_b, (void *)busy_loop);

    phase_unstarted();
    phase_spin();
    phase_blocked("wait", wait_thread);
    phase_blocked("sleep", sleep_thread);

    probe_log("done %s\n", failed ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

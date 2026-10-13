/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * poolstress: does a dead child's share of the JIT pool's head come back, and
 * does the code a later child runs from it work? (PLA-213.) Each pseudo-process
 * copies its ARM64EC images into the pool's head (an x86-64 child: its ntdll
 * copy, kernelbase, kernel32, ucrtbase, FEX and whatever else it loads; an
 * i386 child: ntdll, wow64, wow64win, FEX for WoW64 and win32u). This program
 * starts children one after another, each after the last has exited and the
 * runtime's reuse grace (3 s) has passed, so a child's images can be copied
 * into the ranges the one before it held. The pool's head is read from the
 * app's `title: pool:` lines and the runtime's `[jit-pool] RECLAIM … bump=`
 * lines in the same log.
 *
 *   poolstress.exe [ROUNDS [PAUSE_MS]]   (default 6 rounds, 3500 ms)
 *       each round starts `poolstress.exe child` (x86-64) and, when it is
 *       beside this program, `poolstress-i386.exe child` (i386), waits for
 *       each, and then PAUSE_MS. A child must exit 0.
 *   poolstress.exe child
 *       loads user32, shell32, shlwapi, ole32 and advapi32, and calls each
 *       (CharUpperA, wsprintfA, SHGetFolderPathA, PathFindFileNameA,
 *       CoCreateGuid and StringFromGUID2, RegOpenKeyExA and RegQueryValueExA),
 *       the C runtime (qsort, snprintf, strtod) and code written into an RWX
 *       page, and checks every result; exits 0, or 1 with the failed checks
 *       logged. A fault in an image copy ends the child with c0000005.
 *
 * Each line goes to OutputDebugString, stdout and <dir>\poolstress.log; the
 * parent's last line is "poolstress: done ok" (exit 0) or "poolstress: done
 * FAIL" (1).
 *
 * Built with llvm-mingw for x86-64 (poolstress.exe) and i686
 * (poolstress-i386.exe) by tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char dir[MAX_PATH];
static const char *role = "parent";
static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed;

static void probe_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "poolstress: %s: ", role);

    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, line, (DWORD)strlen(line), &written, NULL);
}

static void check(int ok, const char *what)
{
    if (ok) return;
    failed = 1;
    probe_log("CHECK %s: FAIL\n", what);
}

static int compare_ints(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* `mov eax, imm32; ret` in an RWX page, called with 64 values. */
static void check_rwx_code(void)
{
    unsigned char *code = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    int i, wrong = 0;

    check(code != NULL, "VirtualAlloc RWX");
    if (!code) return;
    for (i = 0; i < 64; i++)
    {
        DWORD value = 0x51000000u + (DWORD)i * 0x10101u;
        int (*fn)(void) = (int (*)(void))code;

        code[0] = 0xb8;
        memcpy(code + 1, &value, 4);
        code[5] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), code, 6);
        if ((DWORD)fn() != value) wrong++;
    }
    check(!wrong, "RWX code returns what was written");
    VirtualFree(code, 0, MEM_RELEASE);
}

static int child_main(void)
{
    typedef HRESULT (WINAPI *folder_fn)(HWND, int, HANDLE, DWORD, LPSTR);
    typedef LPSTR (WINAPI *findname_fn)(LPCSTR);
    HMODULE user32 = LoadLibraryA("user32.dll"), shell32 = LoadLibraryA("shell32.dll");
    HMODULE shlwapi = LoadLibraryA("shlwapi.dll"), ole32 = LoadLibraryA("ole32.dll");
    HMODULE advapi32 = LoadLibraryA("advapi32.dll");
    char buf[MAX_PATH], text[64];
    int values[256], i, sorted = 1;
    unsigned seed = GetCurrentProcessId();
    GUID guid;
    WCHAR guid_text[64];
    HKEY key;
    DWORD type, size;

    check(user32 && shell32 && shlwapi && ole32 && advapi32, "LoadLibrary user32 shell32 shlwapi ole32 advapi32");

    strcpy(text, "pool stress");
    CharUpperA(text);
    check(!strcmp(text, "POOL STRESS"), "CharUpperA");
    wsprintfA(text, "%d-%s-%x", 42, "ok", 0xbeef);
    check(!strcmp(text, "42-ok-beef"), "wsprintfA");

    if (shell32)
    {
        folder_fn get_folder = (folder_fn)GetProcAddress(shell32, "SHGetFolderPathA");
        check(get_folder && get_folder(NULL, CSIDL_WINDOWS, NULL, 0, buf) == S_OK && strchr(buf, '\\'),
              "SHGetFolderPathA(CSIDL_WINDOWS)");
    }
    if (shlwapi)
    {
        findname_fn find_name = (findname_fn)GetProcAddress(shlwapi, "PathFindFileNameA");
        check(find_name && !strcmp(find_name("C:\\a\\b\\pool.exe"), "pool.exe"), "PathFindFileNameA");
    }

    check(SUCCEEDED(CoInitializeEx(NULL, COINIT_MULTITHREADED)), "CoInitializeEx");
    check(CoCreateGuid(&guid) == S_OK && StringFromGUID2(&guid, guid_text, 64) == 39 && guid_text[0] == '{',
          "CoCreateGuid and StringFromGUID2");
    CoUninitialize();

    size = sizeof(buf);
    check(!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key)
          && !RegQueryValueExA(key, "CurrentVersion", NULL, &type, (BYTE *)buf, &size) && type == REG_SZ && size > 1,
          "RegQueryValueExA CurrentVersion");

    for (i = 0; i < 256; i++) values[i] = (int)((seed = seed * 1103515245 + 12345) >> 8);
    qsort(values, 256, sizeof(*values), compare_ints);
    for (i = 1; i < 256; i++) if (values[i - 1] > values[i]) sorted = 0;
    check(sorted, "qsort");
    snprintf(text, sizeof(text), "%.3f|%5d|%s", 2.5, 17, "x");
    check(!strcmp(text, "2.500|   17|x"), "snprintf");
    check(strtod("1234.5e-1", NULL) == 123.45, "strtod");

    check_rwx_code();

    probe_log("pid %lu checks %s\n", GetCurrentProcessId(), failed ? "FAIL" : "ok");
    return failed;
}

static DWORD run_child(const char *exe, DWORD *ms)
{
    char cmdline[MAX_PATH * 2];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 0xffffffff, start = GetTickCount();

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    snprintf(cmdline, sizeof(cmdline), "\"%s\\%s\" child", dir, exe);
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        probe_log("CreateProcess(%s) failed: %lu\n", exe, GetLastError());
        return code;
    }
    if (WaitForSingleObject(pi.hProcess, 60000) != WAIT_OBJECT_0)
        probe_log("%s did not exit within 60 s\n", exe);
    else
        GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    *ms = GetTickCount() - start;
    return code;
}

int main(int argc, char **argv)
{
    char path[MAX_PATH], *slash;
    int rounds = 6, round, children = 0, bad = 0, with_i386;
    DWORD pause = 3500;

    GetModuleFileNameA(NULL, path, sizeof(path));
    strcpy(dir, path);
    if ((slash = strrchr(dir, '\\'))) *slash = 0;

    if (argc > 1 && !strcmp(argv[1], "child"))
    {
#ifdef _WIN64
        role = "child-x64";
#else
        role = "child-i386";
#endif
        return child_main();
    }

    snprintf(path, sizeof(path), "%s\\poolstress.log", dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (argc > 1) rounds = atoi(argv[1]);
    if (argc > 2) pause = (DWORD)atoi(argv[2]);
    snprintf(path, sizeof(path), "%s\\poolstress-i386.exe", dir);
    with_i386 = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
    probe_log("%d rounds, %lu ms after each child, i386 children %s\n", rounds, pause, with_i386 ? "yes" : "no");

    for (round = 1; round <= rounds; round++)
    {
        static const char *const exes[2] = { "poolstress.exe", "poolstress-i386.exe" };
        int k;

        for (k = 0; k < (with_i386 ? 2 : 1); k++)
        {
            DWORD ms = 0, code = run_child(exes[k], &ms);

            children++;
            if (code) bad++;
            probe_log("round %d %s exit=%lx %lu ms\n", round, exes[k], code, ms);
            Sleep(pause);
        }
    }
    probe_log("%d children, %d failed\n", children, bad);
    probe_log("done %s\n", bad ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return bad ? 1 : 0;
}

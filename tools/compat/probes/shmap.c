/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * shmap: writable views of small sections (PLA-307). FMOD Ex makes each of
 * its semaphores a 0x14-byte pagefile-backed section (CreateFileMapping of
 * INVALID_HANDLE_VALUE, PAGE_READWRITE) and maps it with MapViewOfFile
 * (FILE_MAP_WRITE); a NULL view leaves BioShock's FMOD thread close a NULL
 * semaphore. On iOS a host page is 16 KiB and a guest page 4 KiB.
 *
 * For sections of 0x14, 0x1000, 0x3000, 0x4000 and 0x5000 bytes, pagefile
 * backed ("anon") and backed by a file of that size in the probe's folder
 * ("file"):
 *   map    two FILE_MAP_WRITE views of the section and a third through a
 *          DuplicateHandle'd handle must all map, at different addresses
 *   alias  a byte written through one view, at the start, the middle and
 *          the last byte of the section, must read back through the others
 *   file   (file only) after the views are unmapped and the handles closed,
 *          ReadFile must see the last bytes written
 *
 * Each line goes to OutputDebugString, stdout and <dir>\shmap.log. A line
 * ends "ok" or "BAD"; the last is "shmap: done ok" (exit 0) or
 * "shmap: done FAIL" (1). Built with llvm-mingw for x86-64 (shmap.exe) and
 * i386 (shmap-i386.exe) by tools/compat/probes/build.sh.
 */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed;
static char dir[MAX_PATH];

static void probe_log(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "shmap: ");

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

/* Writes through views[from] at the start, the middle and the last byte and
 * reads each back through every other view; returns 1 when all match. */
static int check_alias(volatile unsigned char *views[3], int from, DWORD size, unsigned char tag)
{
    DWORD offsets[3] = { 0, size / 2, size - 1 };
    int i, v, ok = 1;

    for (i = 0; i < 3; i++)
    {
        unsigned char value = (unsigned char)(tag + i);
        views[from][offsets[i]] = value;
        for (v = 0; v < 3; v++)
            if (views[v][offsets[i]] != value) ok = 0;
    }
    return ok;
}

static void check_section(const char *kind, DWORD size)
{
    char path[MAX_PATH + 32];
    HANDLE file = INVALID_HANDLE_VALUE, section, dup = NULL;
    volatile unsigned char *views[3] = { NULL, NULL, NULL };
    int i, mapped, alias = 0;

    if (!strcmp(kind, "file"))
    {
        static const unsigned char zero[0x1000];
        DWORD left = size, written;

        snprintf(path, sizeof(path), "%s\\shmap-%lx.bin", dir, (unsigned long)size);
        file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           CREATE_ALWAYS, 0, NULL);
        if (file == INVALID_HANDLE_VALUE)
        {
            probe_log("%s 0x%lx: CreateFile error %lu BAD\n", kind, (unsigned long)size, GetLastError());
            failed = 1;
            return;
        }
        while (left)
        {
            DWORD chunk = left < sizeof(zero) ? left : sizeof(zero);
            WriteFile(file, zero, chunk, &written, NULL);
            left -= chunk;
        }
        section = CreateFileMappingA(file, NULL, PAGE_READWRITE, 0, 0, NULL);
    }
    else section = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, NULL);
    if (!section)
    {
        probe_log("%s 0x%lx: CreateFileMapping error %lu BAD\n", kind, (unsigned long)size, GetLastError());
        failed = 1;
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        return;
    }
    DuplicateHandle(GetCurrentProcess(), section, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
    views[0] = MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, 0);
    views[1] = MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, 0);
    views[2] = dup ? MapViewOfFile(dup, FILE_MAP_WRITE, 0, 0, 0) : NULL;
    mapped = views[0] && views[1] && views[2] && views[0] != views[1] && views[1] != views[2];
    probe_log("%s 0x%lx: map %p %p %p (error %lu) %s\n", kind, (unsigned long)size, (void *)views[0],
              (void *)views[1], (void *)views[2], mapped ? 0ul : GetLastError(), verdict(mapped));
    if (mapped)
    {
        alias = check_alias(views, 0, size, 0x10) && check_alias(views, 1, size, 0x20) &&
                check_alias(views, 2, size, 0x30);
        probe_log("%s 0x%lx: alias %s\n", kind, (unsigned long)size, verdict(alias));
    }
    for (i = 0; i < 3; i++)
        if (views[i]) UnmapViewOfFile((void *)views[i]);
    if (dup) CloseHandle(dup);
    CloseHandle(section);
    if (file != INVALID_HANDLE_VALUE)
    {
        /* the last check_alias wrote 0x30, 0x31, 0x32 */
        unsigned char got[3] = { 0, 0, 0 };
        DWORD offsets[3] = { 0, size / 2, size - 1 }, read;
        int ok = mapped && alias;

        for (i = 0; i < 3 && ok; i++)
        {
            SetFilePointer(file, (LONG)offsets[i], NULL, FILE_BEGIN);
            if (!ReadFile(file, &got[i], 1, &read, NULL) || read != 1 || got[i] != 0x30 + i) ok = 0;
        }
        if (mapped && alias)
            probe_log("%s 0x%lx: file %02x %02x %02x %s\n", kind, (unsigned long)size, got[0], got[1], got[2],
                      verdict(ok));
        CloseHandle(file);
        DeleteFileA(path);
    }
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    static const DWORD sizes[] = { 0x14, 0x1000, 0x3000, 0x4000, 0x5000 };
    char path[MAX_PATH + 16], *slash;
    SYSTEM_INFO info;
    unsigned int i;

    GetModuleFileNameA(NULL, dir, sizeof(dir));
    if ((slash = strrchr(dir, '\\'))) *slash = 0;
    snprintf(path, sizeof(path), "%s\\shmap.log", dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    GetSystemInfo(&info);
    probe_log("start: %d-bit, page 0x%lx, granularity 0x%lx\n", (int)sizeof(void *) * 8,
              (unsigned long)info.dwPageSize, (unsigned long)info.dwAllocationGranularity);

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) check_section("anon", sizes[i]);
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) check_section("file", sizes[i]);

    probe_log("done %s\n", failed ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

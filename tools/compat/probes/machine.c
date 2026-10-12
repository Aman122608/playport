/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * machine: what a guest sees of the machine values wineboot writes at every
 * start (PLA-168), whether xaudio2_9redist.dll loads (PLA-167), and the
 * CurrentControlSet link and a typelib registration (PLA-166). The app runs
 * no wineboot, so these come from the prefix seed and the host's session
 * start only.
 *
 *   system    GetSystemInfo, GetNativeSystemInfo, IsWow64Process2, CPUID's
 *             vendor, signature and brand string, and the rate RDTSC counts
 *             at against QueryPerformanceCounter over 200 ms (wineboot's x86
 *             ~MHz is that rate)
 *   inputs    what wineboot reads to write the CPU values, as this process
 *             sees them: SystemCpuInformation (level, revision, feature bits),
 *             ProcessorInformation's MHz, the SMBIOS processor's vendor and
 *             version strings and its thread count
 *   env       NUMBER_OF_PROCESSORS, PROCESSOR_ARCHITECTURE/ARCHITEW6432,
 *             PROCESSOR_IDENTIFIER/LEVEL/REVISION, COMPUTERNAME, USERDOMAIN,
 *             LOGONSERVER, USERNAME, USERPROFILE, CLIENTNAME, SESSIONNAME
 *   name      GetComputerNameW, GetComputerNameExW (NetBIOS, DNS host name)
 *   registry  HARDWARE\DESCRIPTION\System (Identifier, the CentralProcessor and
 *             FloatingPointProcessor subkeys, CentralProcessor\0's values),
 *             Session Manager\Environment's processor values, HKCU's Volatile
 *             Environment
 *   import    run xaudio2redist.exe (it imports xaudio2_9redist.dll's
 *             XAudio2Create statically) as a child from this folder (no DLL:
 *             logged, desktop Proton ends it with c0000135) and, when
 *             redist\xaudio2_9redist.dll is there (a title's own copy, put
 *             there by hand), from redist\, where it must exit 0. It runs
 *             before xaudio2: under desktop Proton a child started after this
 *             process had made an XAudio2 engine did not return from
 *             XAudio2Create
 *   xaudio2   LoadLibrary("xaudio2_9redist.dll") by the search path: neither
 *             Wine nor Proton has one, so it fails there too (logged, not a
 *             CHECK); then redist\xaudio2_9redist.dll by its path, when
 *             present, XAudio2Create and Release (a CHECK)
 *   controlset  the same value through CurrentControlSet and ControlSet001,
 *             and whether CurrentControlSet is a link
 *   typelib   LoadRegTypeLib of stdole2 (00020430-...-46, 2.0)
 *
 * The CHECK lines are what desktop Wine and Proton give after wineboot; the
 * values themselves are logged so the two can be compared line by line.
 * Each line goes to OutputDebugString, stdout and <dir>\machine.log; the last
 * line is "machine: done ok" (exit 0) or "machine: done FAIL" (1).
 * Built with llvm-mingw for x86-64 (machine.exe) and i386
 * (machine-i386.exe) by tools/compat/probes/build.sh.
 *
 * xaudio2redist.exe is the same file built with -DXAUDIO2_IMPORT: a program
 * whose only import beyond kernel32 is xaudio2_9redist.dll, as the EOS SDK's
 * EOSSDK-Win64-Shipping.dll has it.
 */
#include <windows.h>
#include <winternl.h>
#include <powerbase.h>
#include <objbase.h>
#include <oleauto.h>
#include <cpuid.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef HRESULT (WINAPI *xaudio2_create_fn)(void **engine, UINT32 flags, UINT32 processor);

#ifdef XAUDIO2_IMPORT

__declspec(dllimport) HRESULT WINAPI XAudio2Create(void **engine, UINT32 flags, UINT32 processor);

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    void *engine = NULL;
    HRESULT hr = XAudio2Create(&engine, 0, 0);
    char line[128];

    snprintf(line, sizeof(line), "xaudio2redist: XAudio2Create hr %08lx engine %p\n", (unsigned long)hr, engine);
    OutputDebugStringA(line);
    if (SUCCEEDED(hr) && engine) (*(ULONG (WINAPI **)(void *))(*(void ***)engine + 2))(engine);
    return SUCCEEDED(hr) ? 0 : 2;
}

#else

static HANDLE log_file = INVALID_HANDLE_VALUE;
static int failed;
static SYSTEM_INFO system_info;

static void probe_log(const char *fmt, ...)
{
    char line[1024];
    va_list ap;
    DWORD written;
    int n = snprintf(line, sizeof(line), "machine: ");

    va_start(ap, fmt);
    vsnprintf(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    OutputDebugStringA(line);
    printf("%s", line);
    fflush(stdout);
    if (log_file != INVALID_HANDLE_VALUE) WriteFile(log_file, line, (DWORD)strlen(line), &written, NULL);
}

static void check(const char *what, int ok)
{
    probe_log("CHECK %s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) failed = 1;
}

static void system_phase(void)
{
    SYSTEM_INFO native;
    USHORT process = 0, host = 0;
    BOOL (WINAPI *wow64_2)(HANDLE, USHORT *, USHORT *) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process2");
    unsigned int a, b, c, d, regs[12];
    char vendor[13], brand[49];

    GetSystemInfo(&system_info);
    GetNativeSystemInfo(&native);
    probe_log("system: GetSystemInfo arch %u level %u revision %04x processors %lu mask %llx\n",
              system_info.wProcessorArchitecture, system_info.wProcessorLevel, system_info.wProcessorRevision,
              system_info.dwNumberOfProcessors, (unsigned long long)system_info.dwActiveProcessorMask);
    probe_log("system: GetNativeSystemInfo arch %u level %u revision %04x processors %lu\n",
              native.wProcessorArchitecture, native.wProcessorLevel, native.wProcessorRevision,
              native.dwNumberOfProcessors);
    if (wow64_2 && wow64_2(GetCurrentProcess(), &process, &host))
        probe_log("system: IsWow64Process2 process %04x native %04x\n", process, host);

    __cpuid(0, a, b, c, d);
    memcpy(vendor, &b, 4);
    memcpy(vendor + 4, &d, 4);
    memcpy(vendor + 8, &c, 4);
    vendor[12] = 0;
    __cpuid(1, a, b, c, d);
    probe_log("system: cpuid vendor %s signature %08x (family %u model %u stepping %u)\n", vendor, a,
              ((a >> 8) & 0xf) + ((a >> 8 & 0xf) == 0xf ? (a >> 20) & 0xff : 0),
              ((a >> 4) & 0xf) | ((a >> 8 & 0xf) >= 6 ? (a >> 12) & 0xf0 : 0), a & 0xf);
    __cpuid(0x80000002, regs[0], regs[1], regs[2], regs[3]);
    __cpuid(0x80000003, regs[4], regs[5], regs[6], regs[7]);
    __cpuid(0x80000004, regs[8], regs[9], regs[10], regs[11]);
    memcpy(brand, regs, 48);
    brand[48] = 0;
    probe_log("system: cpuid brand \"%s\"\n", brand);

    {
        LARGE_INTEGER f, q0, q1;
        unsigned long long t0, t1;

        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&q0);
        t0 = __builtin_ia32_rdtsc();
        Sleep(200);
        QueryPerformanceCounter(&q1);
        t1 = __builtin_ia32_rdtsc();
        probe_log("system: rdtsc %.1f MHz against QueryPerformanceCounter (%lld Hz)\n",
                  (double)(t1 - t0) * (double)f.QuadPart / (double)(q1.QuadPart - q0.QuadPart) / 1e6,
                  (long long)f.QuadPart);
    }
}

/* SYSTEM_CPU_INFORMATION (winternl.h has no definition of it). */
struct cpu_information
{
    USHORT architecture, level, revision, maximum_processors;
    ULONG feature_bits;
};

struct processor_power
{
    ULONG number, max_mhz, current_mhz, mhz_limit, max_idle_state, current_idle_state;
};

/* The n-th string of an SMBIOS structure (1-based), or "". */
static const char *smbios_string(const BYTE *hdr, BYTE n, const BYTE *end)
{
    const char *p = (const char *)hdr + hdr[1];

    if (!n) return "";
    while (--n && (const BYTE *)p < end) p += strlen(p) + 1;
    return (const BYTE *)p < end ? p : "";
}

static void inputs_phase(void)
{
    NTSTATUS (WINAPI *query)(int, void *, ULONG, ULONG *) =
        (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    struct cpu_information sci;
    struct processor_power power[64];
    UINT len = GetSystemFirmwareTable('RSMB', 0, NULL, 0);
    BYTE *buf = len ? malloc(len) : NULL;

    memset(&sci, 0, sizeof(sci));
    if (query && !query(1 /* SystemCpuInformation */, &sci, sizeof(sci), NULL))
        probe_log("inputs: SystemCpuInformation arch %u level %u revision %04x features %08lx\n", sci.architecture,
                  sci.level, sci.revision, sci.feature_bits);
    memset(power, 0, sizeof(power));
    if (!CallNtPowerInformation(ProcessorInformation, NULL, 0, power,
                                sizeof(power[0]) * (system_info.dwNumberOfProcessors < 64 ? system_info.dwNumberOfProcessors : 64)))
        probe_log("inputs: ProcessorInformation cpu0 max %lu MHz current %lu MHz\n", power[0].max_mhz,
                  power[0].current_mhz);
    else
        probe_log("inputs: ProcessorInformation failed\n");
    if (buf && GetSystemFirmwareTable('RSMB', 0, buf, len) == len && len > 8)
    {
        const BYTE *p = buf + 8, *end = buf + len;
        while (p + 4 <= end && p[0] != 127)
        {
            const BYTE *next = p + p[1];
            if (p[0] == 4 && p[1] >= 0x28)
                probe_log("inputs: SMBIOS processor vendor \"%s\" version \"%s\" threads %u\n",
                          smbios_string(p, p[7], end), smbios_string(p, p[0x10], end), p[0x25]);
            else if (p[0] == 1)
                probe_log("inputs: SMBIOS system manufacturer \"%s\"\n", smbios_string(p, p[4], end));
            while (next + 1 < end && (next[0] || next[1])) next++;
            p = next + 2;
        }
    }
    else
        probe_log("inputs: no SMBIOS table (%u bytes)\n", len);
    free(buf);
}

static int env_value(const char *name, char *out, DWORD len)
{
    DWORD n = GetEnvironmentVariableA(name, out, len);
    if (!n || n >= len)
    {
        out[0] = 0;
        probe_log("env: %s (absent)\n", name);
        return 0;
    }
    probe_log("env: %s=%s\n", name, out);
    return 1;
}

static void env_phase(void)
{
    static const char *const others[] = { "PROCESSOR_ARCHITEW6432", "PROCESSOR_IDENTIFIER", "USERNAME",
                                          "USERPROFILE", "CLIENTNAME", "SESSIONNAME" };
    char value[512], what[128];

    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++) env_value(others[i], value, sizeof(value));
    check("env NUMBER_OF_PROCESSORS is GetSystemInfo's count",
          env_value("NUMBER_OF_PROCESSORS", value, sizeof(value)) &&
              strtoul(value, NULL, 10) == system_info.dwNumberOfProcessors);
    check("env PROCESSOR_ARCHITECTURE is the process's (AMD64 or x86)",
          env_value("PROCESSOR_ARCHITECTURE", value, sizeof(value)) &&
              !strcmp(value, sizeof(void *) == 8 ? "AMD64" : "x86"));
    snprintf(what, sizeof(what), "%u", system_info.wProcessorLevel);
    check("env PROCESSOR_LEVEL is GetSystemInfo's level",
          env_value("PROCESSOR_LEVEL", value, sizeof(value)) && !strcmp(value, what));
    snprintf(what, sizeof(what), "%04x", system_info.wProcessorRevision);
    check("env PROCESSOR_REVISION is GetSystemInfo's revision",
          env_value("PROCESSOR_REVISION", value, sizeof(value)) && !strcmp(value, what));
    check("env COMPUTERNAME set", env_value("COMPUTERNAME", value, sizeof(value)));
    check("env USERDOMAIN set", env_value("USERDOMAIN", value, sizeof(value)));
    check("env LOGONSERVER set", env_value("LOGONSERVER", value, sizeof(value)));
}

static void name_phase(void)
{
    WCHAR name[256];
    DWORD len = 256;
    BOOL ok = GetComputerNameW(name, &len);

    probe_log("name: GetComputerNameW %s \"%ls\"\n", ok ? "ok" : "failed", ok ? name : L"");
    check("name GetComputerNameW", ok);
    len = 256;
    if (GetComputerNameExW(ComputerNameDnsHostname, name, &len)) probe_log("name: DnsHostname \"%ls\"\n", name);
    len = 256;
    if (GetComputerNameExW(ComputerNameDnsDomain, name, &len)) probe_log("name: DnsDomain \"%ls\"\n", name);
}

/* One value, logged as the registry holds it; returns 1 if present. */
static int reg_value(HKEY root, const char *path, const char *name)
{
    HKEY key;
    DWORD type, size;
    BYTE data[512];
    LONG rc = RegOpenKeyExA(root, path, 0, KEY_READ, &key);

    if (!rc)
    {
        size = sizeof(data) - 2;
        rc = RegQueryValueExA(key, name, NULL, &type, data, &size);
        RegCloseKey(key);
    }
    if (rc)
    {
        probe_log("registry: %s [%s] (absent, %ld)\n", path, name, rc);
        return 0;
    }
    if (type == REG_DWORD)
        probe_log("registry: %s [%s] dword %lu (0x%lx)\n", path, name, *(DWORD *)data, *(DWORD *)data);
    else if (type == REG_SZ || type == REG_EXPAND_SZ)
        probe_log("registry: %s [%s] \"%.*s\"\n", path, name, (int)size, (char *)data);
    else
        probe_log("registry: %s [%s] type %lu, %lu bytes\n", path, name, type, size);
    return 1;
}

static DWORD subkey_count(const char *path)
{
    HKEY key;
    DWORD count = 0;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key)) return (DWORD)-1;
    RegQueryInfoKeyA(key, NULL, NULL, NULL, &count, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    RegCloseKey(key);
    return count;
}

static void registry_phase(void)
{
    static const char system[] = "HARDWARE\\DESCRIPTION\\System";
    static const char cpu0[] = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    static const char env[] = "System\\CurrentControlSet\\Control\\Session Manager\\Environment";
    static const char *const env_names[] = { "NUMBER_OF_PROCESSORS", "PROCESSOR_ARCHITECTURE",
                                             "PROCESSOR_IDENTIFIER", "PROCESSOR_LEVEL", "PROCESSOR_REVISION" };
    static const char *const volatile_names[] = { "USERDOMAIN", "LOGONSERVER", "CLIENTNAME", "SESSIONNAME",
                                                  "HOMESHARE", "USERNAME", "USERPROFILE" };
    DWORD cpus = subkey_count("HARDWARE\\DESCRIPTION\\System\\CentralProcessor");
    DWORD fpus = subkey_count("HARDWARE\\DESCRIPTION\\System\\FloatingPointProcessor");
    int env_ok = 1;

    reg_value(HKEY_LOCAL_MACHINE, system, "Identifier");
    reg_value(HKEY_LOCAL_MACHINE, system, "SystemBiosDate");
    probe_log("registry: CentralProcessor subkeys %ld, FloatingPointProcessor subkeys %ld\n", (long)cpus,
              (long)fpus);
    check("registry one CentralProcessor key a processor", cpus == system_info.dwNumberOfProcessors);
    check("registry CentralProcessor\\0 Identifier", reg_value(HKEY_LOCAL_MACHINE, cpu0, "Identifier"));
    check("registry CentralProcessor\\0 VendorIdentifier", reg_value(HKEY_LOCAL_MACHINE, cpu0, "VendorIdentifier"));
    check("registry CentralProcessor\\0 ProcessorNameString",
          reg_value(HKEY_LOCAL_MACHINE, cpu0, "ProcessorNameString"));
    check("registry CentralProcessor\\0 ~MHz", reg_value(HKEY_LOCAL_MACHINE, cpu0, "~MHz"));
    reg_value(HKEY_LOCAL_MACHINE, cpu0, "FeatureSet");
    for (size_t i = 0; i < sizeof(env_names) / sizeof(env_names[0]); i++)
        env_ok &= reg_value(HKEY_LOCAL_MACHINE, env, env_names[i]);
    check("registry Session Manager\\Environment processor values", env_ok);
    for (size_t i = 0; i < sizeof(volatile_names) / sizeof(volatile_names[0]); i++)
        reg_value(HKEY_CURRENT_USER, "Volatile Environment", volatile_names[i]);
}

/* Load path (or name, by the search path), XAudio2Create, Release; returns 1 if all worked. */
static int xaudio2_load(const char *what, const char *name)
{
    HMODULE dll = LoadLibraryA(name);
    DWORD error = dll ? 0 : GetLastError();
    xaudio2_create_fn create = dll ? (xaudio2_create_fn)GetProcAddress(dll, "XAudio2Create") : NULL;
    void *engine = NULL;
    HRESULT hr = E_FAIL;
    char path[MAX_PATH] = "";

    if (dll) GetModuleFileNameA(dll, path, sizeof(path));
    probe_log("xaudio2: %s LoadLibrary %p \"%s\" (error %lu), XAudio2Create %p\n", what, (void *)dll, path, error,
              (void *)create);
    if (!create) return 0;
    hr = create(&engine, 0, 0);
    probe_log("xaudio2: %s XAudio2Create hr %08lx engine %p\n", what, (unsigned long)hr, engine);
    if (SUCCEEDED(hr) && engine) (*(ULONG (WINAPI **)(void *))(*(void ***)engine + 2))(engine);
    return SUCCEEDED(hr);
}

static void xaudio2_phase(const char *dir)
{
    char path[MAX_PATH + 48];

    xaudio2_load("search path", "xaudio2_9redist.dll");
    snprintf(path, sizeof(path), "%s\\redist\\xaudio2_9redist.dll", dir);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
        probe_log("xaudio2: no redist\\xaudio2_9redist.dll beside the probe\n");
    else
        check("xaudio2 a title's xaudio2_9redist.dll loads and XAudio2Create succeeds",
              xaudio2_load("redist", path));
}

/* Run xaudio2redist.exe in folder; returns its exit code, or -1. */
static DWORD run_child(const char *folder)
{
    char cmd[MAX_PATH + 48];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = (DWORD)-1;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    snprintf(cmd, sizeof(cmd), "\"%s\\xaudio2redist.exe\"", folder);
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, folder, &si, &pi))
    {
        if (WaitForSingleObject(pi.hProcess, 60000) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        probe_log("import: %s\\xaudio2redist.exe exit %08lx\n", folder, code);
    }
    else
        probe_log("import: CreateProcess %s\\xaudio2redist.exe failed %lu\n", folder, GetLastError());
    return code;
}

static void import_phase(const char *dir)
{
    char redist[MAX_PATH + 16], dll[MAX_PATH + 48];

    run_child(dir);
    snprintf(redist, sizeof(redist), "%s\\redist", dir);
    snprintf(dll, sizeof(dll), "%s\\xaudio2_9redist.dll", redist);
    if (GetFileAttributesA(dll) != INVALID_FILE_ATTRIBUTES)
        check("import a program importing a title's xaudio2_9redist.dll starts and exits 0", run_child(redist) == 0);
}

static void controlset_phase(void)
{
    static const char sub[] = "\\Control\\ComputerName\\ComputerName";
    char current[128], set1[128];
    HKEY key;
    int link = 0;

    snprintf(current, sizeof(current), "System\\CurrentControlSet%s", sub);
    snprintf(set1, sizeof(set1), "System\\ControlSet001%s", sub);
    reg_value(HKEY_LOCAL_MACHINE, current, "ComputerName");
    reg_value(HKEY_LOCAL_MACHINE, set1, "ComputerName");
    if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet", REG_OPTION_OPEN_LINK, KEY_READ, &key))
    {
        DWORD type, size = 0;
        link = !RegQueryValueExA(key, "SymbolicLinkValue", NULL, &type, NULL, &size) && type == REG_LINK;
        RegCloseKey(key);
    }
    probe_log("controlset: CurrentControlSet is %s\n", link ? "a link (SymbolicLinkValue)" : "a plain key");
    probe_log("controlset: Select\\Current, ControlSet001 subkeys %ld\n",
              (long)subkey_count("System\\ControlSet001"));
    reg_value(HKEY_LOCAL_MACHINE, "System\\Select", "Current");
}

static void typelib_phase(void)
{
    static const GUID stdole2 = { 0x00020430, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
    ITypeLib *lib = NULL;
    HRESULT hr;

    CoInitialize(NULL);
    hr = LoadRegTypeLib(&stdole2, 2, 0, 0, &lib);
    probe_log("typelib: LoadRegTypeLib(stdole2 2.0) hr %08lx\n", (unsigned long)hr);
    if (lib) lib->lpVtbl->Release(lib);
    lib = NULL;
    hr = LoadTypeLib(L"stdole2.tlb", &lib);
    probe_log("typelib: LoadTypeLib(stdole2.tlb) hr %08lx\n", (unsigned long)hr);
    if (lib) lib->lpVtbl->Release(lib);
    CoUninitialize();
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char dir[MAX_PATH], path[MAX_PATH + 16], *slash;

    GetModuleFileNameA(NULL, dir, sizeof(dir));
    if ((slash = strrchr(dir, '\\'))) *slash = 0;
    snprintf(path, sizeof(path), "%s\\machine.log", dir);
    log_file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    probe_log("start: %d-bit\n", (int)sizeof(void *) * 8);

    system_phase();
    inputs_phase();
    env_phase();
    name_phase();
    registry_phase();
    import_phase(dir);
    xaudio2_phase(dir);
    controlset_phase();
    typelib_phase();

    probe_log("done %s\n", failed ? "FAIL" : "ok");
    if (log_file != INVALID_HANDLE_VALUE) CloseHandle(log_file);
    return failed;
}

#endif

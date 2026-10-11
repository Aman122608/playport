/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dotnet_fakedlls_test.c — dotnet_fakedlls.c on the Linux host (pp test):
 * the inf's [DefaultInstall] and [DefaultInstall.ntamd64] sections linked
 * into a scratch C:\windows, each side into its own DLL set; a relink after
 * a bundle move; a real file kept; lines of another dirid, an unsafe path
 * or a missing builtin skipped. With an argument, the shipped inf
 * (Wine Mono's support/dotnetfakedlls.inf) must link fusion.dll on both sides.
 */
#include "dotnet_fakedlls.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    fputs(text, f);
    fclose(f);
}

static void mk(const char *root, const char *rel)
{
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/%s", root, rel);
    mkdir(p, 0755);
}

/* The link target of root/rel, or "" if it is no link. */
static const char *target(const char *root, const char *rel)
{
    static char t[PATH_MAX];
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/%s", root, rel);
    ssize_t n = readlink(p, t, sizeof(t) - 1);
    t[n > 0 ? n : 0] = 0;
    return t;
}

static int exists(const char *root, const char *rel)
{
    char p[PATH_MAX];
    struct stat st;
    snprintf(p, sizeof(p), "%s/%s", root, rel);
    return !lstat(p, &st);
}

static const char INF[] =
    ";; .INF script for installing .NET fakedlls\r\n"
    "[version]\r\n"
    "signature=\"$CHICAGO$\"\r\n"
    "\r\n"
    "[DefaultInstall]\r\n"
    "WineFakeDlls=FakeDlls\r\n"
    "\r\n"
    "[DefaultInstall.ntamd64]\r\n"
    "WineFakeDlls = FakeDlls64\r\n"
    "\r\n"
    "[FakeDlls]\r\n"
    "10,Microsoft.NET\\Framework\\v2.0.50727,fusion.dll\r\n"
    "10,Microsoft.NET\\Framework\\v4.0.30319,fusion.dll\r\n"
    "10,Microsoft.NET\\Framework\\v3.0\\windows communication foundation,servicemodelreg.exe\r\n"
    "11,,fusion.dll\r\n"
    "10,Microsoft.NET\\..\\..\\escape,fusion.dll\r\n"
    "10,Microsoft.NET\\Framework\\v4.0.30319,missing.dll\r\n"
    "\r\n"
    "[FakeDlls64]\r\n"
    "10,Microsoft.NET\\Framework64\\v4.0.30319,fusion.dll\r\n";

int main(int argc, char **argv)
{
    char root[PATH_MAX], base[PATH_MAX], inf[PATH_MAX], win[PATH_MAX];
    char d32[PATH_MAX], d64[PATH_MAX], err[512], expect[PATH_MAX];
    const char *tmp = getenv("TMPDIR");
    snprintf(root, sizeof(root), "%s/dotnet-fakedlls-XXXXXX", tmp && tmp[0] ? tmp : ".");
    if (!mkdtemp(root)) { perror("mkdtemp"); return 1; }
    if (!realpath(root, base)) { perror("realpath"); return 1; }
    snprintf(inf, sizeof(inf), "%s/dotnetfakedlls.inf", base);
    snprintf(win, sizeof(win), "%s/windows", base);
    snprintf(d32, sizeof(d32), "%s/i386-windows", base);
    snprintf(d64, sizeof(d64), "%s/arm64ec-windows", base);
    mk(base, "windows");
    mk(base, "i386-windows");
    mk(base, "arm64ec-windows");
    put(inf, INF);
    char p[PATH_MAX];
    snprintf(p, sizeof(p), "%s/fusion.dll", d32); put(p, "i386");
    snprintf(p, sizeof(p), "%s/servicemodelreg.exe", d32); put(p, "i386");
    snprintf(p, sizeof(p), "%s/fusion.dll", d64); put(p, "x64");

    CHECK(dotnet_fakedlls_link(inf, win, d32, d64, err, sizeof(err)) == 4);
    printf("%s\n", err);
    CHECK(strstr(err, "3 lines skipped") != NULL);
    snprintf(expect, sizeof(expect), "%s/fusion.dll", d32);
    CHECK(!strcmp(target(win, "Microsoft.NET/Framework/v2.0.50727/fusion.dll"), expect));
    CHECK(!strcmp(target(win, "Microsoft.NET/Framework/v4.0.30319/fusion.dll"), expect));
    snprintf(expect, sizeof(expect), "%s/servicemodelreg.exe", d32);
    CHECK(!strcmp(target(win, "Microsoft.NET/Framework/v3.0/windows communication foundation/servicemodelreg.exe"),
                  expect));
    snprintf(expect, sizeof(expect), "%s/fusion.dll", d64);
    CHECK(!strcmp(target(win, "Microsoft.NET/Framework64/v4.0.30319/fusion.dll"), expect));
    CHECK(!exists(base, "escape"));
    CHECK(!exists(win, "Microsoft.NET/Framework/v4.0.30319/missing.dll"));

    /* A new bundle path: the links follow it; a real .NET install's file stays. */
    char moved[PATH_MAX];
    snprintf(moved, sizeof(moved), "%s/moved-i386", base);
    CHECK(!rename(d32, moved));
    snprintf(p, sizeof(p), "%s/Microsoft.NET/Framework/v2.0.50727/fusion.dll", win);
    CHECK(!unlink(p));
    put(p, "a real fusion.dll");
    CHECK(dotnet_fakedlls_link(inf, win, moved, d64, err, sizeof(err)) == 3);
    snprintf(expect, sizeof(expect), "%s/fusion.dll", moved);
    CHECK(!strcmp(target(win, "Microsoft.NET/Framework/v4.0.30319/fusion.dll"), expect));
    CHECK(!strcmp(target(win, "Microsoft.NET/Framework/v2.0.50727/fusion.dll"), ""));

    /* An inf with neither install section, and none at all. */
    put(inf, "[version]\nsignature=\"$CHICAGO$\"\n");
    CHECK(dotnet_fakedlls_link(inf, win, d32, d64, err, sizeof(err)) == -1);
    snprintf(p, sizeof(p), "%s/none.inf", base);
    CHECK(dotnet_fakedlls_link(p, win, d32, d64, err, sizeof(err)) == -1);

    /* The shipped inf: fusion.dll, which LoadLibraryShim loads, on both sides. */
    if (argc > 1) {
        char shipped_win[PATH_MAX];
        snprintf(shipped_win, sizeof(shipped_win), "%s/shipped", base);
        mk(base, "shipped");
        int n = dotnet_fakedlls_link(argv[1], shipped_win, moved, d64, err, sizeof(err));
        printf("shipped: %s\n", err);
        CHECK(n > 0);
        CHECK(target(shipped_win, "Microsoft.NET/Framework/v4.0.30319/fusion.dll")[0] != 0);
        CHECK(target(shipped_win, "Microsoft.NET/Framework/v2.0.50727/fusion.dll")[0] != 0);
        CHECK(target(shipped_win, "Microsoft.NET/Framework64/v4.0.30319/fusion.dll")[0] != 0);
    }

    char cmd[PATH_MAX + 16];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", base);
    if (system(cmd)) printf("could not remove %s\n", base);
    if (failures) { printf("dotnet fakedlls: %d failures\n", failures); return 1; }
    printf("dotnet fakedlls: ok\n");
    return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * prefix_registry_test.c — prefix_registry.c on the Linux host (pp test): a seed of marked
 * sections with no header, as PlayportKit's PrefixRegistryFile writes it (decision 0081),
 * merged into a hive wineserver saved.
 */
#include "prefix_registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void put(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static char *get(const char *path)
{
    static char buf[1 << 16];
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(buf, 1, sizeof(buf) - 1, f) : 0;
    if (f) fclose(f);
    buf[n] = 0;
    return buf;
}

static int count(const char *hay, const char *needle)
{
    int n = 0;
    for (const char *p = hay; (p = strstr(p, needle)); p += strlen(needle)) n++;
    return n;
}

int main(void)
{
    const char *tmp = getenv("TMPDIR");
    char dir[1024], hive[1100], seed[1100], err[256];
    snprintf(dir, sizeof(dir), "%s/prefix_registry_test.XXXXXX", tmp && *tmp ? tmp : ".");
    if (!mkdtemp(dir)) return 2;
    snprintf(hive, sizeof(hive), "%s/system.reg", dir);
    snprintf(seed, sizeof(seed), "%s/playport-registry-system.reg", dir);

    put(hive, "WINE REGISTRY Version 2\n;; All keys relative to \\\\Machine\n\n#arch=win64\n\n"
              "[Software\\\\Valve\\\\Steam\\\\Apps\\\\CommonRedist\\\\DirectX\\\\Jun2010] 1791703781\n"
              "#time=1dd5952478785a8\n\"other\"=dword:00000002\n\n"
              "[Software\\\\Game] 1791703781\n\"InstallDir\"=\"C:\\\\Old\"\n\n");
    put(seed, ";; playport:top-up\n[Software\\\\Valve\\\\Steam\\\\Apps\\\\CommonRedist\\\\DirectX\\\\Jun2010]\n"
              "\"dxsetup\"=dword:00000001\n\n"
              ";; playport:top-up\n[Software\\\\Game]\n\"InstallDir\"=\"C:\\\\Games\\\\New\"\n\n"
              ";; playport:top-up\n[Software\\\\Valve\\\\Steam\\\\Apps\\\\CommonRedist\\\\vcredist\\\\2013]\n"
              "\"x64 12.0.30501\"=dword:00000001\n\n");

    int n = prefix_registry_seed(seed, hive, err, sizeof(err));
    char *out = get(hive);
    CHECK(n >= 0);
    CHECK(!strncmp(out, "WINE REGISTRY Version 2\n", 24));                /* the hive's header stays first */
    CHECK(count(out, ";; playport:top-up") == 0);                         /* no marker copied in */
    CHECK(count(out, "\"dxsetup\"=dword:00000001") == 1);                 /* a missing value topped up */
    CHECK(count(out, "\"other\"=dword:00000002") == 1);
    CHECK(count(out, "\"InstallDir\"=\"C:\\\\Old\"") == 1);               /* a held value never changes */
    CHECK(count(out, "C:\\\\Games\\\\New") == 0);
    CHECK(count(out, "[Software\\\\Valve\\\\Steam\\\\Apps\\\\CommonRedist\\\\vcredist\\\\2013]") == 1);   /* a new key whole */
    CHECK(count(out, "\"x64 12.0.30501\"=dword:00000001") == 1);

    n = prefix_registry_seed(seed, hive, err, sizeof(err));               /* again: nothing to add */
    CHECK(n == 0);
    CHECK(count(get(hive), "\"dxsetup\"=dword:00000001") == 1);

    remove(seed);
    remove(hive);
    remove(dir);
    if (failures) return 1;
    printf("prefix registry: headerless marked seed tops up a saved hive, held values kept, idempotent\n");
    return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dotnet_fakedlls.c — Wine Mono's .NET fake DLL directories as links into
 * the runtime (dotnet_fakedlls.h).
 */

#include "dotnet_fakedlls.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define INF_MAX (256 * 1024)

/* The next line of [*pos, end) without its terminator, trimmed; NULL at the end. */
static const char *next_line(const char **pos, const char *end, size_t *len)
{
    const char *p = *pos;
    if (p >= end) return NULL;
    const char *nl = memchr(p, '\n', (size_t)(end - p));
    const char *e = nl ? nl : end;
    *pos = nl ? nl + 1 : end;
    while (p < e && (*p == ' ' || *p == '\t')) p++;
    while (e > p && (e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) e--;
    *len = (size_t)(e - p);
    return p;
}

/* "[name]" is section name (case-insensitive, as setupapi compares). */
static int is_section(const char *line, size_t len, const char *name)
{
    size_t n = strlen(name);
    return len == n + 2 && line[0] == '[' && line[len - 1] == ']' && !strncasecmp(line + 1, name, n);
}

/* Copy the trimmed field [p, e) into out; 0 if it does not fit. */
static int field(char *out, size_t size, const char *p, const char *e)
{
    while (p < e && (*p == ' ' || *p == '\t')) p++;
    while (e > p && (e[-1] == ' ' || e[-1] == '\t')) e--;
    if ((size_t)(e - p) >= size) return 0;
    memcpy(out, p, (size_t)(e - p));
    out[e - p] = 0;
    return 1;
}

/* The value of key in section (the first section list item), or 0. */
static int section_value(const char *buf, const char *end, const char *section, const char *key,
                         char *out, size_t size)
{
    const char *pos = buf, *line;
    size_t len, klen = strlen(key);
    int in = 0;
    while ((line = next_line(&pos, end, &len))) {
        if (len && line[0] == '[') { in = is_section(line, len, section); continue; }
        if (!in || len <= klen || strncasecmp(line, key, klen)) continue;
        const char *eq = line + klen;
        while (eq < line + len && (*eq == ' ' || *eq == '\t')) eq++;
        if (eq >= line + len || *eq != '=') continue;
        const char *v = eq + 1, *comma = memchr(v, ',', (size_t)(line + len - v));
        return field(out, size, v, comma ? comma : line + len);
    }
    return 0;
}

static int mkdirs(const char *path)
{
    char buf[2048];
    if ((size_t)snprintf(buf, sizeof(buf), "%s", path) >= sizeof(buf)) return -1;
    for (char *p = buf + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(buf, 0755) && errno != EEXIST) return -1;
        *p = '/';
    }
    return (mkdir(buf, 0755) && errno != EEXIST) ? -1 : 0;
}

/* A subdirectory of C:\windows as a relative unix path: '\' to '/', no empty,
 * "." or ".." component, no '/' of its own. 0 if unsafe. */
static int safe_subdir(char *dir)
{
    if (!dir[0]) return 0;
    for (char *p = dir; *p; p++) {
        if (*p == '/') return 0;
        if (*p == '\\') *p = '/';
    }
    for (char *c = dir; c; ) {
        char *slash = strchr(c, '/');
        size_t n = slash ? (size_t)(slash - c) : strlen(c);
        if (!n || (n == 1 && c[0] == '.') || (n == 2 && c[0] == '.' && c[1] == '.')) return 0;
        c = slash ? slash + 1 : NULL;
    }
    return 1;
}

typedef struct { int linked, skipped; } tally;

static void link_section(const char *buf, const char *end, const char *section,
                         const char *windows_dir, const char *dll_dir, tally *t)
{
    const char *pos = buf, *line;
    size_t len;
    int in = 0;
    while ((line = next_line(&pos, end, &len))) {
        if (len && line[0] == '[') { in = is_section(line, len, section); continue; }
        if (!in || !len || line[0] == ';') continue;
        const char *e = line + len, *c1 = memchr(line, ',', len);
        const char *c2 = c1 ? memchr(c1 + 1, ',', (size_t)(e - c1 - 1)) : NULL;
        const char *c3 = c2 ? memchr(c2 + 1, ',', (size_t)(e - c2 - 1)) : NULL;
        char dirid[16], dir[512], name[256], src[2048], dst[2048], path[2048];
        struct stat st;
        if (!c2 || !field(dirid, sizeof(dirid), line, c1) || strcmp(dirid, "10") ||
            !field(dir, sizeof(dir), c1 + 1, c2) || !field(name, sizeof(name), c2 + 1, c3 ? c3 : e) ||
            !name[0] || strchr(name, '/') || strchr(name, '\\') || !strcmp(name, ".") ||
            !strcmp(name, "..") || !safe_subdir(dir)) {
            t->skipped++;
            continue;
        }
        if ((size_t)snprintf(src, sizeof(src), "%s/%s", dll_dir, name) >= sizeof(src) ||
            (size_t)snprintf(path, sizeof(path), "%s/%s", windows_dir, dir) >= sizeof(path) ||
            (size_t)snprintf(dst, sizeof(dst), "%s/%s", path, name) >= sizeof(dst) ||
            access(src, F_OK) || mkdirs(path)) {
            t->skipped++;
            continue;
        }
        if (!lstat(dst, &st)) {
            if (!S_ISLNK(st.st_mode)) { t->skipped++; continue; }
            unlink(dst);
        }
        if (symlink(src, dst)) t->skipped++;
        else t->linked++;
    }
}

int dotnet_fakedlls_link(const char *inf_path, const char *windows_dir,
                         const char *dll_dir32, const char *dll_dir64, char *err, size_t errlen)
{
    FILE *f = fopen(inf_path, "rb");
    if (!f) {
        snprintf(err, errlen, "cannot open %s: %s", inf_path, strerror(errno));
        return -1;
    }
    char *buf = malloc(INF_MAX);
    size_t n = buf ? fread(buf, 1, INF_MAX, f) : 0;
    int whole = buf && feof(f);
    fclose(f);
    if (!buf || !n || !whole) {
        free(buf);
        snprintf(err, errlen, "cannot read %s", inf_path);
        return -1;
    }
    const char *end = buf + n;
    char sec32[128], sec64[128];
    int has32 = section_value(buf, end, "DefaultInstall", "WineFakeDlls", sec32, sizeof(sec32));
    int has64 = section_value(buf, end, "DefaultInstall.ntamd64", "WineFakeDlls", sec64, sizeof(sec64));
    tally t32 = {0, 0}, t64 = {0, 0};
    if (has32) link_section(buf, end, sec32, windows_dir, dll_dir32, &t32);
    if (has64) link_section(buf, end, sec64, windows_dir, dll_dir64, &t64);
    free(buf);
    if (!has32 && !has64) {
        snprintf(err, errlen, "%s names no WineFakeDlls section", inf_path);
        return -1;
    }
    snprintf(err, errlen, "%d links (32-bit [%s] %d, 64-bit [%s] %d), %d lines skipped",
             t32.linked + t64.linked, has32 ? sec32 : "", t32.linked, has64 ? sec64 : "", t64.linked,
             t32.skipped + t64.skipped);
    return t32.linked + t64.linked;
}

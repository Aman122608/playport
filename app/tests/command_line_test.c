/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * command_line_test.c — app/SessionRoot/command_line.h on the Linux host (pp test):
 * the session root's command line for a title. The executable is quoted, an
 * argument only when it must be (GoldSrc's hl.exe reads `-game cstrike` from
 * the raw line), and every argument reads back unchanged by CommandLineToArgvW's
 * rules.
 */
#include <stdio.h>
#include <string.h>
#include <uchar.h>

typedef char16_t WCHAR;
#include "../SessionRoot/command_line.h"

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static size_t wlen(const WCHAR *s) { size_t n = 0; while (s[n]) n++; return n; }
static int weq(const WCHAR *a, const WCHAR *b) { return wlen(a) == wlen(b) && !memcmp(a, b, wlen(a) * sizeof(WCHAR)); }

/* CommandLineToArgvW's splitting after argv[0]: 2n backslashes and a quote are n
 * backslashes and a toggle, 2n+1 are n and a literal quote, other backslashes are
 * literal, `""` inside quotes is a literal quote. One argument from *p into out. */
static int next_arg(const WCHAR **p, WCHAR *out)
{
    const WCHAR *s = *p;
    size_t n = 0;
    int quoted = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return 0;
    while (*s && (quoted || (*s != ' ' && *s != '\t'))) {
        size_t slashes = 0;
        while (*s == '\\') { slashes++; s++; }
        if (*s == '"') {
            for (size_t i = 0; i < slashes / 2; i++) out[n++] = '\\';
            if (slashes % 2) { out[n++] = '"'; s++; }
            else if (quoted && s[1] == '"') { out[n++] = '"'; s += 2; }
            else { quoted = !quoted; s++; }
        } else {
            for (size_t i = 0; i < slashes; i++) out[n++] = '\\';
            if (*s && (quoted || (*s != ' ' && *s != '\t'))) out[n++] = *s++;
        }
    }
    out[n] = 0;
    *p = s;
    return 1;
}

static void line_of(WCHAR *line, size_t cap, const WCHAR *exe, const WCHAR **args, int n)
{
    size_t used = 0;
    line[0] = 0;
    CHECK(append_arg(line, cap, &used, exe, 1));
    for (int i = 0; i < n; i++) CHECK(append_arg(line, cap, &used, args[i], 0));
}

int main(void)
{
    WCHAR line[512], got[512];

    /* What Steam passes Counter-Strike: bare arguments after the quoted executable. */
    const WCHAR *cs[] = { u"-steam", u"-game", u"cstrike", u"-soft" };
    line_of(line, 512, u"C:\\Games\\Half-Life\\hl.exe", cs, 4);
    CHECK(weq(line, u"\"C:\\Games\\Half-Life\\hl.exe\" -steam -game cstrike -soft"));

    /* Quoted only when needed; each reads back unchanged. */
    const WCHAR *hard[] = { u"", u"two words", u"say \"hi\"", u"C:\\dir\\", u"C:\\my dir\\", u"a\\\\b", u"tab\there",
                            u"\\\"", u"-x=C:\\path\\file.txt", u"trail\\\\" };
    line_of(line, 512, u"C:\\Games\\A Game\\game.exe", hard, 10);
    CHECK(weq(line, u"\"C:\\Games\\A Game\\game.exe\" \"\" \"two words\" \"say \\\"hi\\\"\" C:\\dir\\ \"C:\\my dir\\\\\" "
                    u"a\\\\b \"tab\there\" \"\\\\\\\"\" -x=C:\\path\\file.txt trail\\\\"));
    const WCHAR *p = line;
    CHECK(next_arg(&p, got) && weq(got, u"C:\\Games\\A Game\\game.exe"));
    for (int i = 0; i < 10; i++) {
        CHECK(next_arg(&p, got));
        if (!weq(got, hard[i])) printf("FAIL round trip of argument %d\n", i), failures++;
    }
    CHECK(!next_arg(&p, got));

    /* Too long for the buffer: refused, not cut. */
    size_t used = 0;
    WCHAR small[8] = { 0 };
    CHECK(append_arg(small, 8, &used, u"abc", 0));
    CHECK(!append_arg(small, 8, &used, u"defgh", 0));
    used = 0;
    CHECK(!append_arg(small, 8, &used, u"abcdefgh", 1));

    if (failures) return 1;
    puts("command line: quoting only when needed, round trips (empty, quotes, trailing backslashes, spaces, tabs), overflow");
    return 0;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// A title's command line, built as Windows programs (and Steam) build one: the
// executable's path in quotes, each argument bare unless it is empty or holds
// a blank or a quote, then quoted with CommandLineToArgvW's escapes. A game that
// reads its own command line by hand, not through argv, sees what it would see
// on Windows: GoldSrc's hl.exe finds no `-game` in `"-game" "cstrike"`.
// The includer defines WCHAR (windows.h; a 16-bit type in app/tests).
#ifndef PLAYPORT_COMMAND_LINE_H
#define PLAYPORT_COMMAND_LINE_H

#include <stddef.h>

static int arg_needs_quotes(const WCHAR *arg)
{
    if (!*arg) return 1;
    for (; *arg; arg++)
        if (*arg == ' ' || *arg == '\t' || *arg == '\n' || *arg == '\v' || *arg == '"') return 1;
    return 0;
}

/* `arg` onto `out` (a space first unless `out` is empty), quoted when `quote`
 * is set or the argument needs it; 0 when it would not fit in `cap`
 * characters with the terminator. */
static int append_arg(WCHAR *out, size_t cap, size_t *used, const WCHAR *arg, int quote)
{
    size_t slashes = 0;
    if (*used && *used + 1 < cap) out[(*used)++] = ' ';
    if (*used + 1 >= cap) return 0;
    if (!quote && !arg_needs_quotes(arg)) {
        for (; *arg; arg++) {
            if (*used + 1 >= cap) return 0;
            out[(*used)++] = *arg;
        }
        out[*used] = 0;
        return 1;
    }
    out[(*used)++] = '"';
    for (; *arg; arg++) {
        if (*arg == '\\') { slashes++; continue; }
        if (*used + 2 * slashes + 2 >= cap) return 0;
        if (*arg == '"') {
            while (slashes) { out[(*used)++] = '\\'; out[(*used)++] = '\\'; slashes--; }
            out[(*used)++] = '\\';
        } else {
            while (slashes) { out[(*used)++] = '\\'; slashes--; }
        }
        out[(*used)++] = *arg;
    }
    if (*used + 2 * slashes + 2 >= cap) return 0;
    while (slashes) { out[(*used)++] = '\\'; out[(*used)++] = '\\'; slashes--; }
    out[(*used)++] = '"';
    out[*used] = 0;
    return 1;
}

/* `tail` onto `out` after a space, as it is: a store's own command-line string
 * (GOG's redistributables, decision 0090), which the program parses itself, nested
 * quotes and all. An empty tail adds nothing. 0 when it would not fit in `cap`
 * characters with the terminator. */
static int append_line(WCHAR *out, size_t cap, size_t *used, const WCHAR *tail)
{
    if (!*tail) return 1;
    if (*used + 2 >= cap) return 0;
    out[(*used)++] = ' ';
    for (; *tail; tail++) {
        if (*used + 1 >= cap) return 0;
        out[(*used)++] = *tail;
    }
    out[*used] = 0;
    return 1;
}

#endif

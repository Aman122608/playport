/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PLAYPORT_JIT_DEBUGGER_H
#define PLAYPORT_JIT_DEBUGGER_H

#define JIT_DEBUGGER_UNKNOWN (-1)
#define JIT_DEBUGGER_NONE 0
#define JIT_DEBUGGER_DETACHED 1
#define JIT_DEBUGGER_ATTACHED 2

/* CS_DEBUGGED survives detach. Only a live P_TRACED process with that
 * signing status may receive a universal JIT breakpoint. Failed/short
 * queries fail closed. This does NOT prove protocol support or prevent a
 * detach between the query and the breakpoint. */
static inline int jit_debugger_judge(int signing_ok, int debugged, int process_ok, int traced)
{
    if (!signing_ok || !process_ok) return JIT_DEBUGGER_UNKNOWN;
    if (debugged && traced) return JIT_DEBUGGER_ATTACHED;
    return debugged ? JIT_DEBUGGER_DETACHED : JIT_DEBUGGER_NONE;
}

/* Pool-acquire errors for a breakpoint refused by the live-tracer gate. */
static inline int jit_debugger_gate(int state)
{
    switch (state) {
    case JIT_DEBUGGER_ATTACHED: return 0;
    case JIT_DEBUGGER_NONE: return -2;
    case JIT_DEBUGGER_DETACHED: return -6;
    default: return -7;
    }
}

/* The same Darwin query serves Swift's method selection and C's brk gates. */
int wine_host_jit_debugger_state(void);

#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The shared decision used by Swift selection and both C breakpoint gates.
 * Darwin csops/sysctl themselves need phone confirmation, not Linux mocks. */
#include "jit_debugger.h"
#include <stdio.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
    /* Neither old signing status nor tracing alone permits a breakpoint. */
    CHECK(jit_debugger_judge(1, 0, 1, 0) == JIT_DEBUGGER_NONE);
    CHECK(jit_debugger_judge(1, 1, 1, 0) == JIT_DEBUGGER_DETACHED);
    CHECK(jit_debugger_judge(1, 0, 1, 1) == JIT_DEBUGGER_NONE);
    CHECK(jit_debugger_judge(1, 1, 1, 1) == JIT_DEBUGGER_ATTACHED);

    for (int signing_ok = 0; signing_ok <= 1; signing_ok++)
        for (int process_ok = 0; process_ok <= 1; process_ok++)
            for (int debugged = 0; debugged <= 1; debugged++)
                for (int traced = 0; traced <= 1; traced++) {
                    int state = jit_debugger_judge(signing_ok, debugged, process_ok, traced);
                    CHECK((jit_debugger_gate(state) == 0) ==
                          (signing_ok && process_ok && debugged && traced));
                    if (!signing_ok || !process_ok) CHECK(state == JIT_DEBUGGER_UNKNOWN);
                }

    CHECK(jit_debugger_gate(JIT_DEBUGGER_NONE) == -2);
    CHECK(jit_debugger_gate(JIT_DEBUGGER_DETACHED) == -6);
    CHECK(jit_debugger_gate(JIT_DEBUGGER_UNKNOWN) == -7);
    CHECK(jit_debugger_gate(99) == -7);

    /* Attach, prepare, unexpected detach: a fresh check blocks the second
     * brk even though CS_DEBUGGED remains set. A failed query also blocks it.
     * This cannot model a detach after the last check, or script support. */
    CHECK(jit_debugger_gate(jit_debugger_judge(1, 1, 1, 1)) == 0);
    CHECK(jit_debugger_gate(jit_debugger_judge(1, 1, 1, 0)) == -6);
    CHECK(jit_debugger_gate(jit_debugger_judge(1, 1, 0, 1)) == -7);
    /* A fresh live attach after a stale flag is allowed (active methods). */
    CHECK(jit_debugger_gate(jit_debugger_judge(1, 1, 1, 1)) == 0);

    printf("jit debugger: %s\n", failures ? "FAILED" : "live tracer required; stale flags and failed queries refused");
    return failures != 0;
}

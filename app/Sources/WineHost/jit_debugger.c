/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "jit_debugger.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <unistd.h>

/* The slim SDK omits <sys/codesign.h>. */
int csops(pid_t pid, unsigned int ops, void *useraddr, size_t usersize);
#define HOST_CS_OPS_STATUS 0
#define HOST_CS_DEBUGGED 0x10000000

int wine_host_jit_debugger_state(void)
{
    uint32_t flags = 0;
    const pid_t pid = getpid();
    int signing_ok = csops(pid, HOST_CS_OPS_STATUS, &flags, sizeof(flags)) == 0;
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
    struct kinfo_proc info = {0};
    size_t len = sizeof(info);
    int process_ok = sysctl(mib, 4, &info, &len, NULL, 0) == 0 && len == sizeof(info);
    return jit_debugger_judge(signing_ok, (flags & HOST_CS_DEBUGGED) != 0,
                              process_ok, (info.kp_proc.p_flag & P_TRACED) != 0);
}

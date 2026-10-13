# 2026-10-12: FEX's CPUID brand string, winedbg on a crash, and ending a killed thread

**Linear:** [PLA-220](https://linear.app/playportdev/issue/PLA-220) (the CPUID brand string read
past FEX's per-CPU data), [PLA-173](https://linear.app/playportdev/issue/PLA-173) (winedbg for
arm64ec), [PLA-200](https://linear.app/playportdev/issue/PLA-200) (an asynchronous abort of a
killed thread). The owner was away. The conservative option was taken at each choice and is
recorded below.

**Changes:**

- `patches/fex` 0026: the CPUID index, and the MIDR on iOS.
- `patches/madeira-unix` 0127: the abort of a killed thread.
- The probes:
  - `tools/compat/probes/machine.c` reads the brand string on a thread for each processor.
  - `tools/compat/probes/childabort.c` gains a `debugger` phase. Its `terminate` phase now checks
    that terminated workers stop.

**IPA:** `Playport-26.5-e89e0ba6.ipa` (dev), sha256
`e89e0ba68063b63fa9b3c16e69fc8b52a6e6237b9799a4a4899ce9c8e4f32463`, built from commit 68fa619
and installed in place over `2bec1330…` (74f1212). Run directories are under
`$PLAYPORT_BUILD/agent-notes/sweep10b/`.

## PLA-220: the brand string

**Cause.**

- CPUID leaves 0x80000002-4 (and 0x1A on a hybrid host) read `PerCPUData[GetCPUID()]`, the entry
  for the core the thread runs on.
- `PerCPUData` has one entry for each host MIDR. On iOS, `patches/fex-port` 0009 gives FEX one
  MIDR, so a thread on any core but 0 read past the end of the vector.
- `RunFunctionName`, the JIT's path for the same leaves, already reduced the index modulo the
  vector's size. The handlers `CPUIDEmu::Function_8000_000[234]h(Leaf)` did not.

**What FEX on Linux reports.**

- The brand string is the product name of the core's MIDR, verbatim, from FEX's table.
- Under Proton on an ARM Linux host, the Windows build reads a MIDR for each `CentralProcessor`
  key, so every core has an entry.
- On iOS the one MIDR was 0, which names no core: "Unknown ARM CPU". Apple's implementer with
  part 0 is "Apple Silicon" in FEX's table.

**Fix (fex 0026).**

- The three brand-string handlers and leaf 0x1A take `GetCPUID() % PerCPUData.size()`, as
  `RunFunctionName` does, so every core past the known ones reports the first core's data. This
  is the conservative option: no topology leaf changes. Leaf 1's logical-processor count still
  comes from the one MIDR, as before.
- On iOS the one MIDR is `0x61000000` (Apple, part 0), so the brand string is "Apple Silicon".
  That is the `ProcessorNameString` the host writes (decision
  [0098](../decisions/0098-machine-values-at-session-start.md)).
- Nothing else reads the iOS MIDR. The LRCPC2 erratum check matches Arm's implementer only.

**Desktop Wine 11.18** (scratch prefix, the same probes):

- x86-64 and i386 read "AMD Ryzen 7 9700X 8-Core Processor" on all 16 processors, 8,000 reads.
- `done ok`.

**Phone** (`cpuid`, `cpuid-i386` imports, IPA `e89e0ba6…`):

| Run | What | Result |
| --- | --- | --- |
| `cpuid-i386` | brand on the main thread and on 6 threads, 500 reads each | "Apple Silicon" on every read; the threads ran on processors 0-3; `CHECK … ok`, `done ok` |
| `cpuid-x64` | the same, x86-64 | "Apple Silicon" on the main thread and on the first two threads (1,000 reads, processors 0-3); the third thread ended the probe with `c0000005` (below) |
| `cpuid-x64-2` | the same again | the same, so the crash is deterministic |

Before, on `2bec1330…` (sweep 2.10a), the x86-64 probe read `"H�0"` and then `"H���v!"`, and
the i386 probe read `"��E�"`.

**Found: the x86-64 third thread's crash, which is not this change.**

- The probe's third thread got the TEB and CPU area of its first thread, which had exited
  (`TEB 0x7038034000`, `ChpeV2CpuAreaInfo 0x7035730000`, in both runs).
- At its first `GetCurrentProcessorNumber` it faulted in FEX's dispatcher. Its guest RIP was in
  ntdll's x86-64 syscall stub, with `rax=0x9e` (`NtGetCurrentProcessorNumber`), and it wrote
  to address 0 (`x0=0`).
- The i386 probe reused one TEB six times without a fault.
- A follow-up, for the supervisor to file: **an x86-64 thread on a reused TEB faults at its first
  `syscall` stub**. Neither fex 0026 nor madeira-unix 0127 touches thread start or syscalls.

## PLA-173: winedbg

**What happens now.**

- **x86-64.** kernelbase's `start_debugger` runs `winedbg --auto PID EVENT`. `system32` has no
  `winedbg.exe` (decision [0082](../decisions/0082-system32-programs.md) leaves it out of the
  arm64ec build), so it logs `Couldn't start debugger … (2)` and the title ends with its
  exception code. Earlier runs show this, for example `20261007T155953` and `20261006T153256`.
- **i386.** `syswow64` has the i386 `winedbg.exe` (decision 0071), and a WoW64 crash starts it.
  - Half-Life on 2026-10-11 (run `20261011T174219`): winedbg started as a pseudo-process, read
    the threads' contexts, and called `NtDebugActiveProcess`, which failed with `c0000022`.
  - winedbg then exited -1, and the title ended with `c0000005`.

**Why the attach fails.**

- wineserver's `debugger_attach` sets the debuggee's PEB `BeingDebugged` flag through
  `write_process_memory`. On iOS that reads the task port from `process->trace_data`, which a
  pseudo-process never has.
- `mach_ios.c`'s `get_process_port` keeps it 0 on purpose: making it `mach_task_self()` turned
  on cross-process reads and writes, and Steam's start regressed.
- So no debugger can attach to a pseudo-process. winedbg's backtrace also needs
  `ReadProcessMemory` and debug events, which go the same way.

**Phone** (`childabort-10b2`, phase `debugger`):

- An i386 child writes to NULL with the default error mode.
- kernelbase started `syswow64\winedbg.exe --auto 48 84`, whose `NtDebugActiveProcess` failed
  with `c0000022`.
- The child ended with `c0000005` after 703 ms. `CHECK debugger child ends with c0000005: ok`.
- Desktop Wine 11.18 gives the same result after 571 ms. There winedbg attaches and loads the
  child's symbols through dbghelp.

**Decision (conservative): no change.**

- An arm64ec `winedbg.exe` would start and then fail to attach, as the i386 one does.
- Building it means reconfiguring and rebuilding the arm64ec Wine tree.
- Each start costs JIT pool head space (decision 0094, on top of the child's images).
- A crashed title never hangs on it today: x86-64 has no debugger to wait for, and i386's exits
  at once.
- For a backtrace (Proton's behaviour), the server first needs debugging of a pseudo-process:
  - attach, without the PEB write through a task port;
  - the debug events;
  - `ReadProcessMemory` in one task.

  That is a follow-up, not a staging change.

## PLA-200: a killed thread

**Cause.**

- When `kill_thread` kills a thread that is not waiting in the server, it sends SIGQUIT, and
  upstream's `quit_handler` ends the thread. iOS delivers no signal to a thread:
  `send_thread_signal` has no task port, so it does nothing.
- A killed thread therefore ran on until its next server call or fault. This covers a
  `TerminateThread`'ed worker, and the threads of a killed or crashed child.

**Fix (madeira-unix 0127).** On iOS, `send_thread_signal(SIGQUIT)` calls ntdll's
`ios_abort_thread_in_guest_code`, which works like this:

- It stops the thread with `thread_suspend`.
- It diverts the thread only at a point where it holds nothing:
  - its pc is in code FEX translated (the JIT pool outside every image copy);
  - its TEB shows no FEX lock stamp and no `InSyscallCallback`.
- To divert it, it sets the pc to `user_mode_abort_thread`, with the thread's syscall frame, as
  `quit_handler` does outside a syscall.
- Anywhere else (host code, a syscall, a PE module, FEX itself) the thread is let go, and the
  server tries again on a timer: 1 ms, backing off to 32 ms.
- A diverted thread is checked again until its Mach thread is gone, so a diversion that something
  else undid is made again.
- After 2,000 tries, about a minute, the thread is left running, as before.

Nothing between the suspend and the resume takes a lock or allocates. The JIT pool lookup is the
lock-free one the profiler uses.

**Phone** (`childabort-10b2`, phases `debugger crash terminate`, IPA `e89e0ba6…`):

| Killed threads | Count | Outcome |
| --- | --- | --- |
| workers of the crashed children (4 in the debugger child, 4 in each of three crash children) | 16 | 8 diverted and ended (`[async-abort] … ended after 2-17 tries`); 4 ended by themselves; 4 still waiting in `NtWaitForAlertByThreadId` (an in-process wait, 0 % CPU) after a minute, left as they were |
| `terminate` victims, round 1 | 4 | none diverted; the phase did not finish (below) |

- Before, the threads of a crashed child that made no server call kept running, busy
  (`2026-10-12-dxdiag-child-abort`). Now no killed child thread runs on busy.
- The crash phase's parent workers kept going: `CHECK crash progress: ok`.

**The `terminate` phase hangs in FEX, and the same hang happens without this change.**

- The first victim (`00c0`) faulted in `libwow64fex` just before its kill: a NULL write through
  its TEB's `+0x1788` field.
- It then kept FEX's lock (`[census-hold] … HOLDS fexlock`), so every thread of the probe spun on
  that lock, the main thread too.
- The run on the old IPA (`5399f034…`, run `$PLAYPORT_BUILD/ui-runs/20261012T022350`) shows the
  same: victim `00b8` faulted the same way just before its kill, on the same TEB address
  (`0x70b88c0000`), and the phase stopped.
- So the victims were not in translated code when the server tried. The WoW64 fault (a write
  through a NULL pointer read from TEB `+0x1788` in `libwow64fex`, in a busy `TerminateThread`
  victim) is a follow-up. It may share a cause with the x86-64 one above; that is not shown.
- The probe now waits until each victim has counted once before it terminates it. An earlier run
  without that wait (`childabort`) stopped with every thread of the probe in an in-process wait.
  A thread ended during its start keeps what it holds then (the loader lock among them), on
  Windows too.

**Kept.**

- On the phone, the change ends the busy threads of crashed children, which was the issue's case.
  A thread is touched only where `quit_handler` would end it, and only in translated code.
- The gates below ran with it.
- What it does not end:
  - a killed thread spinning or faulting in FEX or ntdll;
  - a killed thread in an in-process wait, which `kill_thread`'s wakeup does not reach.

  Both are follow-ups.

## Gates (IPA `e89e0ba6…`)

| Run | What | Result |
| --- | --- | --- |
| `gate-hk` | Hollow Knight `app-367520 --until first-frame+10 --shot` | first frame +9.39 s; main menu |
| `gate-p2` | Portal 2 `app-620 --until first-frame+10 --shot` | first frame +6.27 s; Source splash |

No `pp perf` (the owner's testing budget). Host: `pp test --quick` passed; `pp names` and
`pp secrets` clean.

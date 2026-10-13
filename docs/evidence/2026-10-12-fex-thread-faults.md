# 2026-10-12: an x86-64 thread in the session's ntdll, and a terminated WoW64 thread

**Linear:** [PLA-224](https://linear.app/playportdev/issue/PLA-224) (an x86-64 thread faulted at
its first syscall stub), [PLA-223](https://linear.app/playportdev/issue/PLA-223) (a busy
`TerminateThread` victim faulted in `libwow64fex` and kept FEX's lock). The owner was away. The
conservative option was taken at each choice and is recorded below.

**Changes:**

- `patches/madeira-unix` 0128: the stale-pointer heal resolves a child's DLLs for the child.
- `patches/fex` 0027: `BTCpuThreadTerm` (WoW64) keeps another thread's FEX state on iOS.
- `patches/fex` 0028: a `WritePriorityMutex` write hold is counted in TEB `Instrumentation[2]`.
- `patches/madeira-unix` 0129: the real suspend and a killed thread's abort leave a thread with
  that count alone.

**IPA:** `Playport-26.5-5584d3fe.ipa` (dev), sha256
`5584d3fecc293a6e69e5aa8b582298c3a2a9a1e31115ff41b34cf9d255672daa`, built from commit 473ee87
and installed in place. An intermediate IPA, `fcd4afc6…` (0128 and 0027 only), was installed
for the first probe runs. Run directories are under `$PLAYPORT_BUILD/agent-notes/sweep10c/`.

## PLA-224: the x86-64 thread's fault

**Not a reused TEB.** The faulting thread did get the TEB of the probe's first thread, but the
fault does not come from that.

**Cause.**

- A pseudo-process child gets its own copy of ntdll in the JIT pool: ntdll's PE range is shared
  by every process, and each copy runs with its own process's data (`[child-ntdll]`). The
  child's other DLLs are mapped at their own addresses, and their pool copies have no owner.
- The heal (`ios_jit_patch_stale_pointer`) rewrites a slot whose PE VA keeps exec-faulting
  (256 times) to its pool equivalent, resolved for the copy's owner. An ownerless copy resolved
  to the session's ntdll.
- FEX's `KiUserExceptionDispatcher` is a `.data` global of the child's `libarm64ecfex` copy and
  holds ntdll's PE VA (rva 0x62678). Every x64 syscall jumps through it. The probe reads the brand
  string 500 times on each thread, and `GetCurrentProcessorNumber` is an x64 syscall stub.
- After 256 syscalls the heal pointed that global at the session's ntdll copy
  (`[stale-heal] 0x73ffe02678 … rewrote 2 slot(s)`: the session's slot and the child's).
- The next syscall ran the session's `KiUserExceptionDispatcher`. Its syscall path set the
  guest RIP to the session copy's `invoke_arm64ec_syscall` (rva 0x8ba30). The child's emulator
  knows only its own copy, so it found that address not executable (`[iOS-xquery] MISS`,
  `NoExec instruction in entry block`). It raised SIGSEGV through `GuestSignal_SIGSEGV`, which
  reads address 0 on purpose: the `c0000005` at `x0=0`.
- Which thread met it was timing: the heal thread runs every 100 ms, and the probe's third
  thread was the first to start after it. The first threads' RIPs in the child's own copy were
  translated (`[pool-rip-fix]`).
- Not a regression of 0127 or 0026. The heal and its owner rule are Madeira's. The per-process
  alias tables are `madeira-unix` 0044. No older IPA was run (the testing budget).

**Fix (madeira-unix 0128).**

- Each pool mapping records the process whose thread copied the image (`loader_peb`, from
  `ios_jit_current_peb()`). Module loads run on the owning process's thread, and the pool
  ledger and the NtProtect pointer sync already rely on this.
- The heal resolves an ownerless copy's slots for that process. An owned copy is resolved for
  its owner, as before. A copy whose process is not known keeps the old resolution.
- The rest of translation is unchanged: only the heal reads the new field.

**Phone** (`cpuid` and `cpuid-i386` imports):

| Run | IPA | Result |
| --- | --- | --- |
| `cpuid-x64` | `fcd4afc6…` | the heal rewrote 2 slots at the same point as before; all 6 threads read the brand string; `CHECK cpuid brand string … ok`, `done ok` |
| `cpuid-i386` | `fcd4afc6…` | `CHECK cpuid … ok`, `done ok` |
| `cpuid-x64-final` | `5584d3fe…` | the same as `cpuid-x64`: 6 threads, `done ok` |

## PLA-223: the terminated WoW64 thread

**What TEB+0x1788 is.**

- `TEB+0x1788` is `ChpeV2CpuAreaInfo`. WoW64 FEX points it at its `FrontendThreadData::CpuArea`.
- The faulting instructions are `ldr x9,[x8,#0x1788]; mov w10,#0xff; strb w10,[x9]`. That is
  `LockJITContext`'s `CpuArea()->InSimulation = -1` with a NULL CPU area.

**Cause.**

- `BTCpuThreadTerm` for another thread suspends it and reads its context, "so we know internal
  JIT locks aren't being held". It then clears the thread's CPU area and frees its state.
- On iOS the suspend does not always stop the thread. The server holds a thread only at a safe
  point. Otherwise it defers and retries on a timer, and `NtGetContextThread` does not wait for
  that (`[srv-suspend] … count 0->1` with no hold; `deferred=79` holds in a 120-victim run).
- So the victim ran on and met its NULL CPU area. It kept FEX's lock while that fault was
  handled, and every thread spun on the lock.
- Upstream Wine suspends a thread inside FEX only cooperatively, through the CPU area's
  `SuspendDoorbell`. The iOS real suspend does not.

**First fix (fex 0027), and what it showed** (`childabort-term`, IPA `fcd4afc6…`):

- On iOS, a thread terminated by another now leaves FEX's thread registry and keeps its state.
- No fault: the phase got through 20 of 30 rounds (it hung in round 1 before).
- It then hung on FEX's lock in another way. A victim (`01d4`) got a late real-suspend hold
  (`[real-susp] late hold #5 … wpm_depth=0 cbw=0`) while it write-owned the code invalidation
  mutex (`[fexlock] STUCK … owner tid=01d4`). It was then killed there and never ended.
- The safe point checked FEX's shared holds (`Instrumentation[7]`) and the CodeBufferWriteMutex
  stamp (`Instrumentation[6]`), but not a write hold, which was recorded only in the mutex.

**Second fix (fex 0028, madeira-unix 0129).**

- A write hold counts in `Instrumentation[2]` (`0x16c8`, unused so far) from acquire to release.
- The real suspend's safe point and the killed-thread abort (0127) treat a nonzero count like a
  shared hold: they let the thread go and try again.

**Phone** (`childabort-term2`, phase `terminate`, IPA `5584d3fe…`):

- All 30 rounds ran (21.6 s), with no `TEB+0x1788` fault and no `[fexlock] STUCK`. One
  `[census-hold]` sample caught a running thread holding the lock for a moment.
- The keep-workers made progress: `CHECK terminate progress: ok` (counts 194376-199024; no stale
  rewritten code).
- `CHECK terminated workers stop: FAIL (114 of 120 counted on after they were terminated)`. The
  probe exits 1.
  - The 0127 abort ended 29 victims, each after 2-642 tries. The rest were still busy when the
    probe checked 500 ms after each round. These workers rewrite code all the time, so they are
    mostly in FEX's invalidation path, where the abort does not divert.
  - This is PLA-200's remaining case: a killed thread busy in FEX. Before 0127 all 120 ran on,
    and in 2.10b the phase hung in round 1.

**Shared cause?** No.

- PLA-224 is the heal resolving for the wrong process. It has nothing to do with thread
  teardown or TEB reuse.
- PLA-223 is thread teardown under a suspend that does not stop the thread.

## Kept (conservative)

- **0027 leaks a terminated thread's FEX state.** After 120 terminations the band held 8.9 GB
  of 16 GB, with 122 FEX threads (callret stack, L1 cache, rpmalloc heap).
  - The rpmalloc heap of a cross-terminated thread leaked before as well (`[thr-term] CROSS-TERM`
    in ARM64EC).
  - The new cost is the callret stack (16 MB virtual) and the lookup cache (2 MB) per
    `TerminateThread` of a busy thread.
  - Games rarely terminate more than a few threads, and freeing state under a running thread is
    the fault this fixes.
- **ARM64EC `ThreadTerm` has the same pattern** for x86-64 `TerminateThread`, but it was not
  changed. No x86-64 case was observed, and the probe that would show it is i386 only. This is a
  follow-up.
- **The write-hold stamp** is one TEB store at each write acquire and each release. Write holds
  are rare (invalidation, not the per-block path).

## Gates (IPA `5584d3fe…`)

| Run | What | Result |
| --- | --- | --- |
| `gate-hk` | Hollow Knight `app-367520 --until first-frame+10 --shot` | first frame +9.87 s; main menu |
| `gate-p2` | Portal 2 `app-620 --until first-frame+10 --shot` | first frame +5.81 s; Source splash |

No `pp perf` (the owner's testing budget). Host: `pp test --quick` passed; `pp names` and
`pp secrets` clean.

## Follow-ups

- **A killed thread busy in FEX is not ended for seconds** (PLA-200's remaining case; 114 of 120
  victims). One design: on a kill, set the victim's `SuspendDoorbell`, so FEX brings it to a
  block boundary (`brk #0xCAFE`, `HandleSuspendInterrupt`) and into a syscall where the server
  ends it. That is upstream's cooperative suspend.
- **Free a terminated thread's FEX state once its Mach thread has ended** (0027's leak). The
  abort path knows when the thread has ended.
- **ARM64EC `ThreadTerm`:** the same race as PLA-223 for x86-64 `TerminateThread`. An x86-64
  build of the childabort probe would show it.

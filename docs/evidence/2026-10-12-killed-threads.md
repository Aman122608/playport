# 2026-10-12: killed threads busy in FEX, and the x86-64 TerminateThread race

**Linear:** [PLA-222](https://linear.app/playportdev/issue/PLA-222) (a killed thread busy in FEX
or in an in-process wait is not ended), [PLA-226](https://linear.app/playportdev/issue/PLA-226)
(the ARM64EC `ThreadTerm` race for x86-64 `TerminateThread`),
[PLA-225](https://linear.app/playportdev/issue/PLA-225) (the FEX state that fex 0027 keeps), all
part of [PLA-200](https://linear.app/playportdev/issue/PLA-200). The owner was away. The
conservative option was taken at each choice and is recorded below.

**Changes:**

- `patches/madeira-unix` 0130: the abort of a thread killed alone rings FEX's suspend doorbell,
  and `trap_handler` ends the thread at the doorbell's breakpoint.
- `patches/fex` 0029: ARM64EC `ThreadTerm` keeps another thread's state on iOS, as fex 0027 does
  for WoW64. It also clears the CPU area's doorbell pointer before it frees a thread's own state.
- `tools/compat/probes`: an x86-64 build of childabort (`childabort.exe`).

**IPA:** `Playport-26.5-af75dafe.ipa` (dev), sha256
`af75dafeeedce0eba782375fef394e65466cc54a42c042d37f933dafbbccf784`. It was built from commit
2be70f9 and installed in place. Intermediate IPAs were installed for the probe runs (below). Run
directories are under `$PLAYPORT_BUILD/agent-notes/sweep10d/`.

## PLA-222: a killed thread busy in FEX

**Before.** With madeira-unix 0127, the server ends a killed thread only when it finds the thread
in translated code, holding nothing. These probe workers rewrite code all the time, so they are
mostly in FEX itself. On IPA `5584d3fe…`, 114 of the 120 terminated i386 workers were still
counting 500 ms after their round (sweep 2.10c).

**Fix (madeira-unix 0130).** This is upstream's cooperative suspend (`usr1_handler`), with a
killed flag instead of `suspend_pending`:

- **When the doorbell is rung.** The server's abort (`ios_abort_thread_in_guest_code`) may find
  a thread killed by `TerminateThread` somewhere it cannot divert it from. The server then
  writes, while the thread is suspended:
  - the doorbell in the thread's FEX state (`CHPE_V2_CPU_AREA_INFO::SuspendDoorbell`);
  - a `killed` flag in the thread's `arm64_thread_data`.

  The writes go through `mach_vm_write`.
- **Where the thread stops.** The JIT checks the doorbell at the start of each block and raises
  `brk #0xCAFE` there, after the thread has released FEX's locks.
  - `trap_handler` ends a killed thread at that breakpoint with `user_mode_abort_thread`, as
    `quit_handler` does outside a syscall. This happens before any exception dispatch.
  - A thread whose TEB shows a FEX lock or a syscall callback continues through FEX's suspend
    path. `signal_set_full_context` then ends it with `abort_thread`, without a server call.
- **The abort never diverts a thread stopped at `brk #0xCAFE`.** Such a thread may be inside
  the breakpoint's exception.
- **Only a thread killed alone gets the doorbell** (`!process->is_terminating`). The threads of a
  terminating process, such as a crashed child, keep 0127's abort unchanged.
- **The doorbell stays valid.** It is a field of the thread's FEX state. FEX keeps that state
  for a thread terminated by another (fex 0027, 0029). Before it frees the state of a thread that
  ends itself, FEX clears the pointer: WoW64 clears the CPU area, and ARM64EC the doorbell pointer
  (fex 0029).

**Phone, the final design** (`i386-ct6`, IPA `93330a12…`, the same code as the final IPA,
phases `crash terminate`):

| Check | Result |
| --- | --- |
| `crash` | children 0, 1 and 2 exit with `0xc0000005`; `CHECK crash progress: ok` |
| `terminate` | 30 rounds in 17.5 s |
| `CHECK terminated workers stop` | **ok (0 of 120 counted on after they were terminated)**; it was 114 of 120 before |
| `CHECK terminate progress` | ok (no stale rewritten code) |
| exit | `done ok` |

The same `terminate` result came from `i386-term2` (IPA `5e6fc448…`, the first working form of
the change, without the scope limit).

**Ways tried and rejected on the phone, in order:**

1. **Wake a killed thread in `NtWaitForAlertByThreadId` and end it there** (`i386-ct1`, a
   wine-unix patch that was dropped). The thread's `RtlWaitOnAddress` entry is on its stack, in
   the futex queue that every pseudo-process shares (ml441). After a crashed child's thread was
   ended there, the parent faulted when it unlinked that entry (`ntdll.dll+0x6e588`, a write into
   the child's freed stack). **Killed threads in in-process waits are therefore not woken.** That
   part of PLA-222 stays open, with this design note.
2. **Doorbell with `suspend_pending`, upstream's way** (`i386-ct2`, `i386-term`). The abort's
   divert also caught threads stopped in the `brk #0xCAFE` exception. The SIGTRAP that followed
   then reached the guest as `c000001d` at `user_mode_abort_thread`, and the probe hung. This led
   to the `brk` check above.
3. **Doorbell, ending the thread in `signal_set_full_context`** (`i386-ct3`). A killed thread went
   through the exception dispatch first. It failed a server write with EBADF while it held
   `fd_cache_mutex`, and the process hung. This led to ending the thread in `trap_handler`.
4. **Doorbell for every killed thread, ended in `trap_handler`** (`i386-ct4`, `i386-ct5`).
   - `terminate` passed.
   - In `crash`, the threads of a crashed child ended during its teardown and broke the next
     child's start:
     - `CreateProcess` failed with error 6, or the child's `virtual_map_main_module` failed with
       `c0000008`;
     - once a parent worker's server write failed with EBADF at its own exit.
   - The same `crash` phase on a baseline build of 9ac0515 (IPA `119f1cf2…`, `base-crash`)
     passed. There the children's threads end later, through 0127's divert.
   - A further change was tried and dropped (madeira-unix 0131, in commit 485e364): a thread of
     an exited child closed no descriptors at its exit. It did not prevent the failure.
   - This led to the scope limit above.

## PLA-226: x86-64 `TerminateThread`

**Reproduced first** (`x64-term-base`, IPA `5584d3fe…`, the new x86-64 childabort, phase
`terminate`):

- The first victim (`0040`) was suspended in `__ulock_wait2`. ARM64EC `ThreadTerm` freed its
  state and emulator stack (`[thr-term] CROSS-TERM`).
- The thread was released and faulted on its freed emulator stack:
  `SEGV … addr=0x73fd10e920`, then `[exc-stack] cannot deliver exception … aborting thread`.
- Every thread of the process then spun. The phase hung in round 1, and `pp ui` timed out.
- The same probe passes under desktop Wine 11.18 (0 of 120 ran on; `done ok`).

**Fix (fex 0029).** On iOS, ARM64EC `ThreadTerm` for another thread now:

- removes the thread from the registry and the sweep registry;
- skips the stat slot;
- keeps the thread's state and emulator stack.

This is the 0027 treatment. A thread that ends itself is torn down as before, after the doorbell
pointer is cleared.

**Phone after the fix** (`x64-term`, `x64-term2`, `x64-term3`):

- Round 1 and later rounds complete: 4 and 12 `CROSS-TERM … its FEX state is kept`, and the
  victims stop (`Suspending`, `[async-abort] … ended`). No stack fault and no hang.
- The phase still ends early with `c0000005` in a keep worker. This is a separate race, not a
  kill:
  - Two live workers reuse one page address. Worker A frees its RWX code page, and worker B is
    given the same address at once (`[jit-pool] anon RWX 0x703ebe0000 … tid=003c` right after
    `0038` freed it).
  - FEX's executable-range query then misses it: `[iOS-xquery] MISS`,
    `NoExec instruction in entry block`, and a NULL read.
  - The likely cause is that A's free notification removed B's new range. The iOS port dropped
    the mutex around `NotifyMemoryAlloc`/`Free` ("AB-BA deadlock" note).
  - It happens 1 time in about 200 allocations, in all three runs. It is a follow-up and was not
    changed here.

## PLA-225: the kept state

**Not changed.** Freeing a terminated thread's kept FEX state once its Mach thread has ended is
not contained:

- The server's retry timer (0127) knows when the Mach thread is gone, but the state belongs to
  the process's FEX copy (a PE DLL in the JIT pool). The server cannot call it: native code must
  not call an ARM64EC export (ml614).
- From inside FEX, Windows reports such a thread as dead at once, while its Mach thread may still
  run. So FEX has no safe moment to free the state.

The cost is unchanged from 2.10c: about 18 MB of virtual band per cross-terminated thread
(callret stack and lookup cache). ARM64EC now adds its emulator stack and FEX state. After 120
terminations the i386 run showed `band used_mb=7215` of 16384.

**Design for later:** a server-to-process notice, for example a ring in the PEB that FEX drains
at `ThreadInit`/`ThreadTerm`. It would list the TEBs whose Mach threads have ended, so FEX frees
their state on its own thread.

## Gates (final IPA `af75dafe…`, commit 2be70f9)

| Run | What | Result |
| --- | --- | --- |
| `gate-hk` | Hollow Knight `app-367520 --until first-frame+10 --shot` | first frame +9.82 s; main menu (Start Game, Options, Achievements, Extras, Quit Game) |
| `gate-p2` | Portal 2 `app-620 --until first-frame+10 --shot` | first frame +5.91 s; the "powered by Source" splash |

- No `pp perf` (the owner's testing budget).
- Host: `pp test --quick` passed, and `pp names` and `pp secrets` are clean.

## Follow-ups

- **A killed thread in an in-process wait is never ended.** Its wait entry is in the futex
  queue that all pseudo-processes share. Ending it needs it to leave the queue first, for example
  on the PE side.
- **The threads of a terminating process still end only through 0127.** A thread busy in FEX can
  run on, for up to about a minute, after its process has exited. Ringing the doorbell for them
  needs the descriptor and memory teardown of an exited child to tolerate threads that end late.
- **An x86-64 RWX page reused at once between threads loses its executable range**
  (`[iOS-xquery] MISS` after `[jit-pool] anon RWX`). This is the x86-64 childabort `terminate`
  phase above.
- **PLA-225**, with the design above.

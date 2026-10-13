# Evidence: another thread's i386 context, a fresh capture at each suspension (PLA-302, PLA-304, PLA-305)

**Date:** 2026-10-13. **Kind:** the `threadctx` probe on desktop Wine and on the phone (dev app),
a read of the suspend and context paths. **Linear:**
[PLA-302](https://linear.app/playportdev/issue/PLA-302) (WoW64 context),
[PLA-304](https://linear.app/playportdev/issue/PLA-304) (reused capture, with
[PLA-93](https://linear.app/playportdev/issue/PLA-93)),
[PLA-305](https://linear.app/playportdev/issue/PLA-305) (safe-point window),
[PLA-303](https://linear.app/playportdev/issue/PLA-303) (`SetThreadContext` on a running thread);
follow-ups of [2026-10-12-guest-thread-context](2026-10-12-guest-thread-context.md) (PLA-245). The
owner was away; the conservative option was taken at each choice. Run directories are under
`$PLAYPORT_BUILD/ui-runs/`.

**IPAs (dev):** before, `07a0c124db0e383d8e9088ef7e76f131b91b85820c652c9a8107cd2eeb6e97e0`; after,
`bcf6493b533a08602f8438f456f658ac2e773ab04f534b24e5a61e46ddaa3a47` (madeira-unix 0136, 0137,
wine-unix 0036, fex 0037, wine-pe 0042); `b99f63bd` (also the PLA-303 attempt below, not kept);
final, `f39cd151b99d00a0f3ac2f68dcb12d15794d43245881bbffe44099f838d9f2f0` (the same patches as
`bcf6493b`; installed at the end).

## The probe

`tools/compat/probes/threadctx.c` gained check e: a thread counting in `rcx`/`ecx` is suspended,
read, resumed and suspended again four times; each read must show the counter it has reached by
then. Imported as `dir-threadctx 3` and `dir-threadctx-i386 3`. Version 4 (`dir-threadctx 4`,
`dir-threadctx-i386 4`) closes the phase-b loop with an unconditional `jmp` back, for the reason
in PLA-303 below; the checks are otherwise the same. The earlier imports hold the earlier versions.

| Check | desktop Wine 11.18, 64 / 32 (v3 and v4) | phone 64, before | phone 64, after | phone i386, before | phone i386, after |
| --- | --- | --- | --- | --- | --- |
| a: never-run thread | ok | ok | ok | ok | ok |
| d: `SetThreadContext` on a never-run thread | ok | ok | ok | ok | ok |
| b: spinning thread: `rip` in the loop, `rsp` on its stack, `rax` the marker | ok 16/16 | ok 16/16 | ok 16/16 | all zero (0/16) | **ok 16/16** |
| c: thread in `WaitForSingleObject` / `Sleep`: `rip` in an image, `rsp` on its stack | ok | ok | ok | `eip=0 esp=0` | **ok** (ntdll) |
| e: a second suspension sees the counter move on | ok 3/3 | BAD 0/3 | **ok 3/3** | BAD (zeros) | **ok 3/3** |
| d: `rax` set on the spinning thread reaches it | ok | BAD | BAD | BAD | BAD |

Runs: `s11q-probe-before`, `s11q-probe32-before` (`07a0c124`, v3); `s11q-probe-after`,
`s11q-probe32-after` (`bcf6493b`, v3); `s11q-probe-final`, `s11q-probe32-final` (`f39cd151`, v4, the
same results). Desktop logs: `.work/agent-notes/s11q/desktop-*.log`.
`[guest-ctx32]` lines show the i386 conversion, for example `host pc=0x1268212ac
sp=0x702fc5f330 -> eip=0x40145c esp=0x175ff50 (host registers)`.

## PLA-302: WoW64 (i386)

- The server filled the guest side of its Mach capture from the CHPE CPU area's `ContextAmd64`.
  A WoW64 thread has none (FEX's WoW64 module leaves it empty), so `GetThreadContext` from i386
  code got zeros for any thread that was not waiting in its own suspend handler.
- Wine's WoW64 keeps an i386 thread's context in the WoW64 CPU area
  (`TEB->TlsSlots[WOW64_TLS_CPURESERVED]`, `get_cpu_area`). FEX stores its state there whenever the
  thread leaves the emulator for a syscall or a unix call, and loads it back on the way in.
  **madeira-unix 0136** reads that context for an i386 process: a waiting thread reports its
  guest `eip` in ntdll and its guest stack (check c).
- For a thread inside the emulator that context is its last syscall's. **fex 0037**
  `BTCpuIosGuestContext`, the WoW64 counterpart of fex 0035, replaces the control and integer
  registers (as far as the caller asked for them) with the state `ReconstructWowContext` would
  make: in JIT code from the captured host registers and the block's RIP table, in the dispatcher
  and elsewhere in FEX from the spilled state. **wine-pe 0042** has wow64's `NtGetContextThread`
  pass another thread's context through it. A context that differs from the target's CPU area is
  one `SetThreadContext` stored and is left alone. Floating point and XMM stay as the CPU area
  had them.

## PLA-304: a fresh capture at each suspension

- Upstream's thread context lives while the thread waits in its suspend handler and goes when it
  is woken. On iOS the server captures it through Mach and the thread never waits for it, so
  `stop_thread` found the old context and returned early: every later suspension of the thread,
  and every `GetThreadContext` of it while it ran, got the first capture (check e, 0/3 before).
- **wine-unix 0036** marks a context captured through Mach and releases it once the thread is no
  longer suspended (at the last resume, and at the next stop of an unsuspended thread). A
  context the thread itself sent (a thread created suspended) is unchanged.

## PLA-305: the safe-point window

- The real-suspend hold (madeira-unix 0010) is taken only outside host code and outside a
  syscall. It read `kernel_stack` from `TEB+0x3a8`, which is not in the TEB since Wine 11, so
  every thread was judged by its pc alone. **madeira-unix 0137** uses the window 0134 uses: the
  1 MiB kernel stack below the syscall frame at `TEB+0x378`. There is no host test: the code is
  built only in the iOS run tree. On the phone it is not separately visible in the probe (a
  waiting thread's pc is host code, which already defers the hold); no check regressed.

## PLA-303: `SetThreadContext` on a running thread: not fixed

- What it takes: on iOS no thread waits in its suspend handler for a context the server captured
  through Mach, so a `SetThreadContext` is stored in that capture and dropped. Upstream's thread
  takes the changed context back when it is woken.
- Tried (`b99f63bd`, runs `s11q-probe-set`, `s11q-probe32-set`, `s11q-probe-set2`,
  `s11q-probe32-set2`; not kept): at the last resume of a thread inside the emulator whose capture
  was changed, the server kept the changed registers in a context waiting for the thread and
  rang its FEX suspend doorbell with `suspend_pending` (upstream's cooperative suspend, as
  PLA-222's `abort_ring_doorbell` does for a killed thread). At its next block FEX stored the guest
  state and `signal_set_full_context` took the suspend in `wait_suspend`, which merged the changed
  registers in: `[set-ctx]` showed the thread resuming at the pc `GetThreadContext` had given.
- Two findings stopped it:
  - FEX checks the doorbell at a block's entry and at an unconditional backward branch, not at a
    conditional backward branch (`CondJump`). The v3 loop (`jz 1b`) never saw the doorbell and
    trapped only after it ended. Upstream FEX is the same.
  - The `rip` that `GetThreadContext` gives for a thread in JIT code comes from the block's RIP
    table, which is not instruction-exact: host pcs before the first entry give the block's first
    instruction (`host pc=0x1281b90b8 -> rip=0x12a351460`, the loop function's entry, while the
    thread was in the loop body). The v4 probe set `rax` and the thread resumed at that `rip`,
    which reloads `rax` with the marker: it stayed in the loop. A caller that saves a context and
    resumes it later (.NET's thread redirection does) would run instructions twice. Applying such a
    context is not safe, so the change was not kept.
- Next step: suspend a thread inside the emulator at a block boundary, as upstream does: at
  `SuspendThread`, ring the doorbell and leave the context pending, so the thread stops at its next
  block with exact state and waits in `wait_suspend`, and a `SetThreadContext` reaches it when it
  is woken. It needs a bound for a thread that never reaches a check (a loop closed by a
  conditional branch, a long FEX compile), falling back to the Mach capture, and a play of the
  Mono titles, whose GC suspends threads all the time.

## Titles on `f39cd151`

- **Hollow Knight** (`s11q-hk`): first frame at +9.1 s; the screenshot shows the main menu. Mono
  read 35 thread contexts, and the server captured each afresh (35 `[srv-getctx]` captures, all
  through the syscall frame, `is_ec=1`). On `s11l-hk` the same 35 reads made 4 captures.
- **Portal 2** (`s11q-p2`): first frame at +5.9 s.
- No other title was played; `pp perf` was not run.

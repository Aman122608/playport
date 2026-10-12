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
wine-unix 0036, fex 0037, wine-pe 0042).

## The probe

`tools/compat/probes/threadctx.c` gained check e: a thread counting in `rcx`/`ecx` is suspended,
read, resumed and suspended again four times; each read must show the counter it has reached by
then. Imported as `dir-threadctx 3` and `dir-threadctx-i386 3` (the earlier imports hold the
earlier versions).

| Check | desktop Wine 11.18, 64 / 32 | phone 64, before | phone 64, after | phone i386, before | phone i386, after |
| --- | --- | --- | --- | --- | --- |
| a: never-run thread | ok | ok | ok | ok | ok |
| d: `SetThreadContext` on a never-run thread | ok | ok | ok | ok | ok |
| b: spinning thread: `rip` in the loop, `rsp` on its stack, `rax` the marker | ok 16/16 | ok 16/16 | ok 16/16 | all zero (0/16) | **ok 16/16** |
| c: thread in `WaitForSingleObject` / `Sleep`: `rip` in an image, `rsp` on its stack | ok | ok | ok | `eip=0 esp=0` | **ok** (ntdll) |
| e: a second suspension sees the counter move on | ok 3/3 | BAD 0/3 | **ok 3/3** | BAD (zeros) | **ok 3/3** |
| d: `rax` set on the spinning thread reaches it | ok | BAD | BAD | BAD | BAD |

Runs: `s11q-probe-before`, `s11q-probe32-before` (`07a0c124`); `s11q-probe-after`,
`s11q-probe32-after` (`bcf6493b`). Desktop logs: `.work/agent-notes/s11q/desktop-*.log`.
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

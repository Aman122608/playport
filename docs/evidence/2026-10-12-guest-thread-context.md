# Evidence: another thread's x86-64 context on iOS (PLA-245)

**Date:** 2026-10-12. **Kind:** a probe on desktop Wine and on the phone (dev app), a read of the
context path, phone runs. **Linear:** [PLA-245](https://linear.app/playportdev/issue/PLA-245), from
JWE ([PLA-41](https://linear.app/playportdev/issue/PLA-41)); Mono suspension is
[PLA-93](https://linear.app/playportdev/issue/PLA-93). The owner was away; the conservative option
was taken at each choice. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

**IPAs (dev):** before, `d34240963adabaf07030d02e42a73600cb803831e8e07c79eeacef1e24ca8f5e` (474934a);
after, `8cdd97f2923dc42cd9d5d2304eb4dc0ae6a0c09d4e9f242b4277914d994154e6` (fex 0035, wine-pe 0041,
madeira-unix 0134; installed at the end).

## What JWE saw

In `s11h-jwe-long` (`warn+seh`), JWE's `fTask W3` suspends thread `00d0` and reads its context
twice. The first read asks for floating point only (`flags=00100008`), so its `rip=0` is not an
answer. The second asks for the control registers and gets `rip=0x1222b5c94`: an address in FEX's
JIT code, where `00d0` was running (`[xterm-where]` gives it as an offset past the end of `S1Probe`'s
image). `rsp` was FEX's stack. The thread was running guest code, not "not yet started".

## The path

- `SuspendThread` and `GetThreadContext` reach the server's `stop_thread`. On iOS no signal reaches
  the thread, so `ios_fill_thread_context` (`build/wineserver/mach_ios.c`) reads its registers with
  `thread_get_state`. The thread's own suspend path, which upstream uses, never runs.
- ARM64EC `NtGetContextThread` asks for the native context and converts it with
  `context_arm_to_x64`: `pc` becomes `rip`. Whatever the thread was executing is reported.
- Upstream Wine on Linux has the thread report itself. In emulated code its SIGUSR1 handler rings
  the CPU area's suspend doorbell, and FEX stops at the next block's `brk #0xcafe` with the state
  `ReconstructThreadState` builds. In a syscall, `usr1_handler` reports the syscall frame
  (`is_inside_syscall`).
- The server already had that syscall-frame substitution (Madeira ml716), opt-in
  (`MADEIRA_CTX_FRAME=1`, never set). It also read `kernel_stack` from `TEB+0x3a8`. Since Wine 11 the
  thread data is not in the TEB, so the substitution could not happen.
- The CPU area's `ContextAmd64` is not maintained, so the "saved AMD64" values are zero. WoW64
  (i386) reads them as its guest context, which is why i386 reports zeros.

## The change

- **fex 0035** `BTCpu64IosGuestContext(teb, context)`. When the target is inside the emulator
  (`InSimulation`), it replaces a captured host context with the guest state, packed in the ARM64EC
  register mapping as `StoreStateToPackedECContext` packs it:
  - in JIT code or the dispatcher, the static registers and NZCV come from the captured registers,
    and `rip` comes from the block's RIP table (`RestoreRIPFromHostPC`);
  - elsewhere in FEX, they come from the spilled state.

  Nothing of the target is written. Its state is read under `ThreadCreationMutex`, taken only with
  `try_lock`, so a held thread cannot block the caller. A pc inside a guest image is left alone:
  that is the syscall frame, or a context `SetThreadContext` stored.
- **wine-pe 0041** ARM64EC `NtGetContextThread` passes another thread's context through it. It asks
  the server for control and integer registers as well, and does not report them in the flags.
- **madeira-unix 0134** The syscall-frame substitution is on. Its window is the 1 MiB kernel stack
  below the frame at `TEB+0x378`. The frame's pc, a JIT-pool copy of ntdll, is given as its PE
  address (`ios_jit_reverse_translate`).

## The probe

`tools/compat/probes/threadctx.c`, built x86-64 and i386 and imported as `dir-threadctx 2` and
`dir-threadctx-i386 2`. The first import (`dir-threadctx`) holds the first version; the
`SetThreadContext` check was rewritten in between.

| Check | desktop Wine 11.18, 64 / 32 | phone 64, before | phone 64, after | phone i386, before and after |
| --- | --- | --- | --- | --- |
| a: never-run thread: `rip` in ntdll, `rcx`/`rdx` (`eax`/`ebx`) the start and parameter | ok (`RtlUserThreadStart` exactly) | ok (ntdll's native entry, `rsp` = StackBase) | the same | ok (exactly) |
| d: `SetThreadContext` on it runs the new start routine | ok | ok | ok | ok |
| b: spinning thread: `rip` in the loop, `rsp` on its stack, `rax` the marker | ok 16/16 | `rip` JIT `0x124541f78`, `rsp` FEX's, `rax` ok | ok 16/16 | all zero |
| d: `rax` set on the spinning thread reaches it | ok | not measured (first version read the server's copy) | BAD | BAD |
| c: thread in `WaitForSingleObject` / `Sleep`: `rip` in an image, `rsp` on its stack | ok (ntdll) | `rip` libsystem_kernel, `rsp` pthread stack | ok (ntdll, ARM64EC code) | `eip=0 esp=0` |

Runs: `s11l-probe-before`, `s11l-probe32-before` (`d3424096`); `s11l-probe-after` (`89a9097a`, an
earlier build, before the pool-address translation); `s11l-probe-final`, `s11l-probe32-final` (`8cdd97f2`).
`[guest-ctx]` lines show the conversion, for example `host pc=0x128429f78 sp=0x73fd210000 ->
rip=0x12a7d146a rsp=0x703488ff78 (host registers)`.

## Titles on `8cdd97f2`

- **Hollow Knight** (`s11l-hk`): first frame at +10.4 s, main menu. Mono suspended threads
  35 times, and every context was in ntdll's ARM64EC code (`is_ec=1`). On `s11i-hk` all 10 were
  libsystem_kernel pcs (`is_ec=0`).
- **Portal 2** (`s11l-p2`): first frame at +6.2 s.
- **Jurassic World Evolution**, Vulkan (`s11l-jwe`): no frame. At +10.5 s the protection showed its
  dialog "Sorry, something went wrong", with `support.codefusion.technology/jpe_f884fd95kfs/?e=88500006`.
  That is the code FM2022 showed in
  [2026-10-12-anti-tamper-replay](2026-10-12-anti-tamper-replay.md): the server refuses online
  re-validation for 24 hours. No thread context was read before it (no `[ec-getctx]` line). JWE
  was not replayed again, so the context check of `s11h-jwe-long` is untested on this build.

## Not changed

- **WoW64 (i386):** a running or waiting thread reads as zeros, because the server takes the guest
  context from the unmaintained CPU area. A fix needs the WoW64 CPU area, which holds the i386
  context saved at a syscall, and FEX's WoW64 module for a thread in JIT code. Follow-up.
- **`SetThreadContext` on a running thread** is stored by the server and never applied on iOS: the
  server does not write a Mach thread's state, and no suspended thread enters `wait_suspend`.
  Follow-up.
- **A captured context is reused.** The server keeps the first capture until the thread makes a
  select with a context, so later suspensions of a thread that makes no server call get its first
  snapshot again (`[srv-getctx]` once per thread). Follow-up, with PLA-93.
- The real-suspend safe-point test (madeira-unix 0010) still reads `TEB+0x3a8` and judges by pc
  alone (`frame_unknown`).
- XMM registers in JIT code come from the spilled state, because the capture has no NEON state.

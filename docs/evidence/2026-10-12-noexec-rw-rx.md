# Evidence: code made executable by NtProtectVirtualMemory reaches FEX again (PLA-203)

**Date:** 2026-10-12. **Kind:** phone runs (dev app) with a probe import, log analysis, a desktop
Wine run, host tests. **Linear:** [PLA-203](https://linear.app/playportdev/issue/PLA-203); titles
[PLA-114](https://linear.app/playportdev/issue/PLA-114) (Football Manager 2022),
[PLA-41](https://linear.app/playportdev/issue/PLA-41) (Jurassic World Evolution),
[PLA-115](https://linear.app/playportdev/issue/PLA-115) (Duck Paradox, Mono).
**Changes:** `patches/wine-pe` 0038 (commit 9d40a49), the probe `tools/compat/probes/protect.c`.
The owner was away, so the conservative option was taken at each choice below. **IPA:**
`Playport-26.5-ad449e16.ipa` (dev), sha256
`ad449e166357303f1ff96320cb5a176fea2aa2b9cebb1f9b3654d8b336493fda`, built from 9d40a49 and installed
in place. Every phone run below is on it. Run directories are under
`$PLAYPORT_BUILD/agent-notes/speedrun/noexec/`.

## Summary

- **Root cause:** a stuck syscall-callback guard in the ARM64EC ntdll, not FEX's executable
  tracker. FEX learns about executable memory from Wine's ARM64EC syscall wrappers
  (`dlls/ntdll/signal_arm64ec.c`). `NtAllocateVirtualMemory`, `NtProtectVirtualMemory`,
  `NtFreeVirtualMemory` and `NtMapViewOfSection` call FEX's `NotifyMemoryAlloc`,
  `NotifyMemoryProtect` and similar callbacks. FEX's `InvalidationTracker` then inserts or removes
  the range in `XIntervals`, and the decoder's `QueryGuestExecutableRange` (the `[iOS-xquery]` log)
  answers from those intervals. Images are registered through `NotifyMapViewOfSection` and the
  loader's `NotifyImageMap`. Each wrapper runs its callbacks only after
  `enter_syscall_callback()`, which sets the thread's `InSyscallCallback` and refuses when it is
  already set.
- `NtProtectVirtualMemory` has a Madeira diagnostic, `[exec-req]`, which logs the first 64
  protects that ask for execute. It made its own syscall and post-notification and then
  `return`ed **without `leave_syscall_callback()`**. Every process's first `[exec-req]` is the
  loader's protect of the main image (in the probe, `#1 addr=0x127411000 size=0x1c00
  new_prot=20`). From then on that thread's wrappers took the raw syscall with no FEX
  notification, until something cleared the flag. FEX's exception path does clear it
  (`ResetToConsistentState`).
- **Why RWX worked and RW→RX did not:** the probe's `exec` phase allocates
  `PAGE_EXECUTE_READWRITE`. FEX inserts the interval when the allocation notification gets
  through, and while the guard stays stuck no later protect removes it. A `PAGE_READWRITE`
  allocation enters FEX's tracker as non-executable. The protect to `PAGE_EXECUTE_READ` was the
  only event that could make it executable, and that notification was dropped. Result:
  `[iOS-xquery] MISS`, `NoExec instruction in entry block`, FEX's `NoExecOp`, and its
  deliberate read of address 0.
- **The fix (wine-pe 0038):** call `leave_syscall_callback()` on that early return, as the normal
  path does. It is one line, and the diagnostic is kept. Nothing in FEX's tracker or the iOS
  executable-memory design changes. A pure x86-64 range is translated by FEX at its own
  address, and its EXEC is logical (Wine's page protection; ARCHITECTURE, the executable
  pool). FEX therefore needed only the notification, with no extra copy into the pool.
- **Probe:** `exec-rw` (write `mov eax, N; ret` into a `PAGE_READWRITE` page, protect it
  `PAGE_EXECUTE_READ`, call it, protect it back, 4,096 times) now passes on the phone: 74.2 µs a
  round, 0 stale, with no `[iOS-xquery] MISS` in the run. The other phases still pass.
- **Football Manager 2022 changed.** It no longer dies `c0000005` at RIP `0x159782e84` about 5 s
  in. It shows its 550×146 "Football Manager 2022" window at +6.16 s and was alive with no first
  frame when the replay's 180 s wait ran out. **Jurassic World Evolution did not change**: it
  still dies `c0000005` at `0x1488a3cf6` after 7 s. **Duck Paradox** with its compatibility
  default removed (`MADEIRA_KEEP_SMC=0`) did not change either: it still exits `0xf8f08f70` at
  start.
- **A second, older defect, not fixed here: anon JIT aliases outlive their memory.** A range that
  was once made executable through the anonymous-alias path keeps its alias entry after
  `MEM_RELEASE` (virtual_ios: "Aliases are only cleared on process EXIT"). A later
  `PAGE_READWRITE` allocation at the same address is then forced physically read-only
  (ml638 in `set_protection`). Its stores fault into the alias store emulator and land in the
  dead pool slot, not the new memory. FEX then translates zeros (`00 00` = `add [rax], al`) and
  faults at address 1. The probe's new `exec-rw-reuse` phase reproduces it. This is a follow-up
  (below).

## How it was found

The `probe1` log from 2026-10-12 (IPA `7deac3fa…`) shows the protects of the `rx` phase, done
with `PAGE_EXECUTE_READ`, with no `[iOS-xrem] via=protect` when they go back to
`PAGE_READWRITE`. FEX removes an executable range and logs that line on every non-execute
protect that intersects one. So FEX had never been told about the `PAGE_EXECUTE_READ`. The
only `[exec-req]` lines were #1 to #3, though thousands of execute protects followed. Both
facts mean the wrapper was taking the `!enter_syscall_callback()` raw path. In the wrapper, the
`[exec-req]` branch is the one path between `enter_syscall_callback()` and a `return` that does
not call `leave_syscall_callback()`. The other wrappers in the file pair them on every path.

After the fix (`r1/probe`), the same phase logs `[iOS-xrem] via=protect … 0x7036400000-0x7036401000`
when page 0 goes back to `PAGE_READWRITE`. The notification now arrives.

The Madeira comment on `NotifyImageMap` (ml710: "in a child pseudo-process the syscall
notification never arrives -- enter_syscall_callback() refuses, InSyscallCallback having been
left set") describes the same stuck guard. The loader fallback it explains stays. It is
idempotent and still useful while any path can leave the flag set.

## GoldSrc's NoExec is a different case

Counter-Strike's `NoExec instruction in entry block: 7A411FD0`
([2026-10-12](2026-10-12-goldsrc-soft-host-fault.md)) is a genuine NoExec: the game calls a
window procedure in SDL2.dll after unloading that DLL itself, and `[iOS-xrem] via=section`
removes the interval when the image is unmapped. That code is not executable on Windows either.
The probe's case is memory the guest did make executable, which Windows and desktop Wine run.

## The probe

`tools/compat/probes/protect.c` (phases as on [2026-10-12](2026-10-12-ntprotect-sync.md)), with two
changes for this issue:

- `exec-rw` (when named) now runs **first**, right after `usd`, on address space no earlier
  phase has made executable.
- `exec-rw-reuse` (when named, which also runs `exec-rw`) runs the same phase again at the end.
  Its allocation lands on the range the `data`, `rx` and `exec` phases made executable and
  freed.

The arguments are a game-page setting (`--settings 'dir-protect 2:{"arguments":"exec-rw-reuse"}'`).
The rebuilt `protect.exe` was put in `Documents/Import/protect` and imported again through the UI.
The import came in as a second title, `dir-protect 2`. The old `dir-protect`, with the previous
build, stays.

| Where | IPA | Run | usd read | data | rx | image | exec round | exec-rw round | exec-rw-reuse |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| desktop Wine 11.18 | — | — | 0.4 ns | 0.6 µs | 0.7 µs | 0.6 µs | 1.4 µs | 1.4 µs, ok | 1.4 µs, ok |
| phone, before | `7deac3fa…` | `ntprotect/probe1` | 13.9 µs | 18.0 µs | 18.6 µs | 15.9 µs | — | NoExec, `c0000005` | — |
| phone, old probe | `ad449e16…` | `r1/probe` | 15.1 µs | 17.5 µs | 18.3 µs | 18.0 µs | 81.0 µs | (at the end, on a freed alias range) zeros, `c0000005` | — |
| phone, new probe | `ad449e16…` | `r3/probe` | 15.3 µs | 12.2 µs | 18.5 µs | 18.5 µs | 74.8 µs | **74.2 µs, ok** | zeros, `c0000005` |

In `r1/probe` and `r3/probe` the run that reuses the range fails the same way. Page 0 runs. When
page 0 goes back to `PAGE_READWRITE`, its host page is forced read-only because a dead alias
covers it. The code written to page 1 is routed to the alias's pool slot. FEX translates page 1
from its own address and finds zeros:

```text
[guest-exact] ml885 block=0x123fd43a0 rip=0x702dc71000 x86=00 00 00 00 00 00 00 00 …
[store-noalias] #1 rev=ml348 addr=0x1 insn=0x19400106 … NO pool/anon alias
```

No `[iOS-xquery] MISS` appears in either run. The NoExec is gone, and what is left is the alias
lifetime.

## Phone runs

| Run | Title | Result |
| --- | --- | --- |
| `r1/install` | — | installed `ad449e16…` in place |
| `r1/probe` | `dir-protect` (old build), `exec-rw` | all checks `ok` up to `exec`; `exec-rw` at the end crashes on the freed alias range (above) |
| `r1/hk` | Hollow Knight (`app-367520`) | first frame +9.62 s, `ok`; `[iOS-xquery] MISS` 0, `[iOS-noexec]` 0 (as on `57d12a62…`) |
| `r1/p2` | Portal 2 (`app-620`) | first frame +5.53 s, `ok`; MISS 0, noexec 0 |
| `r3/probe` | `dir-protect 2` (new build), `exec-rw-reuse` | `exec-rw` `ok`, then every phase `ok`, then `exec-rw-reuse` crashes (above) |
| `r4/fm22` | Football Manager 2022 (Epic `29c30b4114b247678453c84ae156715d`), Vulkan 16:9 | no `c0000005`; window "Football Manager 2022" 550×146 at +6.16 s; no first frame within `--wait 180` (exit 2); about 0.3 cores, main thread idle at the end |
| `r4/jwe` | Jurassic World Evolution (Epic `373a4372c20540aca7fdd880d27fa49a`), Vulkan 16:9 | unchanged: `c0000005` after 7 s, `AV WRITE of 70D0906BA9` at RIP `0x1488a3cf6` |
| `r4/p2perf` | Portal 2 `pp perf`, The Cold Boot | below |
| `r5/install`, `r5/duck-nosmc` | Duck Paradox (GOG `1816031813`), `runtime` `env.MADEIRA_KEEP_SMC=0` | installed from GOG (1.0 GB, 10 s); unchanged: `[smc-off] … disabled`, SEGV at pc 0, exit `0xf8f08f70` after 1 s |
| `r5/duck-default` | Duck Paradox, compatibility default | `[smc-off] … KEPT`, first frame +5.82 s, `ok`; then uninstalled through the UI (`r5/uninstall`) |

Football Manager 2022's log, before and after, for the same replay: 37,258 → 25,668
`NtProtectVirtualMemory` calls (35,612 → 24,544 of them `PAGE_EXECUTE_READ`) up to where each run
ended, and 17,709 → 5 `Add SMC interval` lines. The 17,709 come from a second thread
(`0038`) in the `57d12a62…` run, which this run never reached. What the game waits for at its
window was not looked at (PLA-114/PLA-122 are separate).

Portal 2, `pp perf --title app-620 --secs 120 --pad first-frame+30:p2-cold-boot --pad
first-frame+70:p2-walk`, window 75-120 s, on the charger, compared with
[2026-10-12 x87](2026-10-12-x87-fild-fistp-m64.md):

| Run | IPA | FPS | GPU ms | M instructions a frame | CPU % | CPU mW |
| --- | --- | --- | --- | --- | --- | --- |
| `perf/pla202-before-1` | `34055093…` | 60.0 | 15.18 | 60.2 | 124 | 221 |
| `perf/pla202-after-1` | `3e81f622…` | 60.0 | 15.04 | 62.6 | 123 | 290 |
| `noexec/r4/p2perf` | `ad449e16…` | 60.0 | 15.19 | 58.6 | 121 | 225 |

The same frame rate, GPU time and CPU work a frame: delivering the dropped notifications costs
Portal 2 nothing measurable.

## Host

`./pp test --quick`: all passed. The first attempt failed once in
`test_notices_gstreamer` tearDown (`Directory not empty` while removing a temporary `.git`), a
cleanup race that the rerun did not show and that is unrelated to this change. Desktop Wine
11.18 runs the new probe with `exec-rw-reuse`, and every check is `ok`.

## What stays open

- **Anon JIT alias entries outlive `MEM_RELEASE`** (follow-up). A JIT that frees a code region and
  gets the same address back as data or as a new code region writes into a dead pool slot. Repro:
  the probe's `exec-rw-reuse`. A fix belongs in the release path (retire an alias the released
  range fully covers). It touches the alias table that FEX's code buffers and Mono's bridge share,
  so it needs its own gate runs. That is why it was not done here.
- **Football Manager 2022** now stops at a small window with no first frame (PLA-114).
- **Jurassic World Evolution's** wild store is unchanged (PLA-122).
- **Duck Paradox** still needs `MADEIRA_KEEP_SMC=1` (PLA-115). Its code region was writable when
  executed (`writable=true` in the [2026-10-11](2026-10-11-mono-backpatch-smc.md) trace), which is
  not the RW→RX pattern, so this fix was not expected to reach it. The run is consistent with
  that.

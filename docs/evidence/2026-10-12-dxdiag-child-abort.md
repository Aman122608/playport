# Evidence: Wine dxdiag on the phone; a child's start or end no longer wedges the parent (PLA-117, PLA-120)

**Date:** 2026-10-12. **Kind:** phone runs (dev app) with a probe import, desktop Wine runs, log
analysis, host tests. **Linear:** [PLA-117](https://linear.app/playportdev/issue/PLA-117) (dxdiag),
[PLA-120](https://linear.app/playportdev/issue/PLA-120) (the allocator lock), title
[PLA-36](https://linear.app/playportdev/issue/PLA-36) (Mortal Kombat Komplete Edition).
**Changes:** `app/tools/prefix-registry.py` and `app/registry/system.reg` (the computer name),
`patches/madeira-unix` 0120 (diagnostics) and 0121, `patches/wine-unix` 0031, the probe
`tools/compat/probes/childabort.c`. The owner was away, so the conservative option was taken at
each choice below. **IPA:** `Playport-26.5-2d99e298.ipa` (dev), sha256
`2d99e2985485f93f8d93c450659583289433262eac6c2fd2810ddd6f8bea7a22`, built from commit 0c51652
and installed in place; the gates and the last probe run are on it. Run directories are under
`$PLAYPORT_BUILD/ui-runs/`.

## Summary

- **PLA-117: dxdiag failed because the prefix has no computer name.** The app runs no wineboot,
  and wineboot's `create_computer_name_keys` is what writes
  `HKLM\System\CurrentControlSet\Control\ComputerName\ActiveComputerName` at every start.
  `GetComputerNameW` reads that key; dxdiagn's `build_systeminfo_tree` returns `E_FAIL` when it
  fails, so `IDxDiagProvider_Initialize` failed, dxdiag logged `DxDiag information collection
  failed`, exited 1 and wrote nothing. The registry seed now carries the keys wineboot writes
  (`ComputerName\ComputerName` and `ActiveComputerName` = `PLAYPORT`, `Tcpip\Parameters`
  `Hostname` = `playport`, `Domain` empty), a fixed name rather than the phone's. dxdiag then
  exits 0 and writes the report, as on desktop Wine.
- The `NtReadVirtualMemory` → `c0000005` in the lead is not the cause: it is dxdiagn's search for
  NetMeeting (`EnumProcessModules` on each process), whose failure dxdiagn ignores.
- Wine's and Proton's dxdiag write only the **System Information** section: no display section and
  no "Dedicated Memory" line (checked in the Wine source at the pin and in Proton 10's
  `dxdiag.exe`). Mortal Kombat reads that line; a desktop Proton run gets the same file without it.
- **PLA-120: the allocator lock is the victim, not the cause.** Each pseudo-process loads its own
  copy of `libwow64fex.dll` (separate `[jit-pool] image … (libwow64fex.dll)` copies, separate
  `.data`), so a child's thread cannot hold the parent's `fex_ios_rpm_lock`. What the processes
  share are host mutexes in the one ntdll, and the probe showed one of them wedged:
  - a starting child's first thread (`wine_ios_child_main` → `virtual_alloc_teb`) held
    `virtual_mutex` and waited for `ios_wow64_windows_lock` in `NtAllocateVirtualMemory`
    (`ios_wow64_route_vm`), while a WoW64 thread held `ios_wow64_windows_lock` and waited for
    `virtual_mutex` in `NtProtectVirtualMemory`: the registry-then-virtual order broken by
    `virtual_alloc_teb`, which commits a TEB under `virtual_mutex`;
  - every other WoW64 thread of every process then waited for `ios_wow64_windows_lock` in its
    next `NtAllocateVirtualMemory` / `NtFreeVirtualMemory`. A parent thread that does that inside
    FEX's allocator (rpmalloc maps its spans under its global lock, rpmalloc-port 0015) keeps the
    allocator lock, and the parent's other threads spin in `fex_ios_rpm_lock` from
    `FEXCore::Allocator::aligned_alloc`: Mortal Kombat's hang.
  - Mortal Kombat's own hang (2026-10-09, IPA `994b287e…`) began when its dxdiag child's only
    thread was aborted during process exit. Its log does not name the lock: the parent's threads
    spin in `fex_ios_rpm_lock`, and one parent thread (`00b0`) is neither spinning nor parked on a
    futex, which is how a thread blocked in a host mutex looked here before 0120. That build
    released the child's WoW64 window under `ios_wow64_windows_lock` after the master socket had
    closed, so a server call there aborted the thread with the lock held; madeira-unix 0110
    (2026-10-11) moved that release before the socket closes, and 0121 gives the lock back at any
    abort. Whether Mortal Kombat went through that lock is the reading most consistent with its
    log, not a proof.
- **The fix (madeira-unix 0121):** `virtual_alloc_teb` commits the TEB outside `virtual_mutex`
  (the slot is the thread's once `teb_block_pos` has moved), and `ios_wow64_windows_lock` is taken
  through `ios_wow64_windows_enter/leave`, which record it with the thread's uninterrupted sections,
  so `abort_thread`, `abort_process` and `process_exit_wrapper` give it back as they give back the
  sections (0041). Of the three approaches in the issue, this is "record an owner and release it
  on abort", applied to the lock the processes share. "Avoid aborting inside the allocator" does
  not apply: no abort was inside it. "Wait for a safe point" would need an asynchronous abort of a
  running thread, which iOS does not give us without a new mechanism; recorded as a follow-up.
- **A crash of the app on the way (wine-unix 0031).** With the dead child's threads still parked on
  its locks, the lock monitor's `[hot-lock]` dump read a page of the child that had lost its
  access: `EXC_BAD_ACCESS` in `ios_alert_waiter_dump` on the pool warmer thread ended the app.
  msync only rejects an unmapped page; the monitor now checks the page's protection with
  `mach_vm_region` before it reads (or, for a reap, writes) a lock word.
- **Diagnostics (madeira-unix 0120).** A `[thread-sample]` line now carries the thread's 64-bit id
  (`tid64`); a thread in `__psynch_mutexwait` also names the mutex and its owner's id, and a
  native thread gets up to six callers from its frame-pointer chain (`fp-bt`). That is what named
  the two mutexes above.

## The probe

`tools/compat/probes/childabort.c`, built for i386 as `childabort-i386.exe`, imported through the
UI (`pp ui --action import:Import/childabort`, title `dir-childabort`). Four busy workers in the
parent allocate (malloc, HeapAlloc, VirtualAlloc) and rewrite and call a small piece of code the
whole time. Phases: `dxdiag` runs `C:\windows\system32\dxdiag.exe /whql:off /t <dir>\dxdiag.txt`
and checks its exit code and file; `crash` runs the probe itself as a child three times, which
starts busy workers and writes to NULL; `terminate` (only when named) ends code-only workers with
`TerminateThread`. After each phase every worker must have made progress within 15 s. Desktop
Wine 11.18 (xvfb, scratch prefix): all phases `ok`, dxdiag exit 0 with a 623-byte report.

| Run | IPA | Result |
| --- | --- | --- |
| `20261012T014512` | `7138d11e…` (before) | dxdiag exit 1, no file; `IDxDiagProvider_Initialize` fails after `NtOpenKey` `c0000034` (the computer name) |
| `20261012T014853` | `7138d11e…` | dxdiag as before; the second crash child's start hangs the parent; one dead child's thread spins in its `fex_ios_rpm_lock+0x6c`, parent threads in `__psynch_mutexwait` |
| `20261012T020156` | `98ca5eab…` (seed, 0120) | dxdiag exit 0, 569-byte report; the app ends in the first crash phase: `EXC_BAD_ACCESS` in `ios_alert_waiter_dump` |
| `20261012T020846` | `b3cea6eb…` (+ wine-unix 0031) | no app crash; the parent hangs at the third crash child; `[thread-sample]` names the `virtual_mutex` / `ios_wow64_windows_lock` cycle (`virtual_alloc_teb` against `NtProtectVirtualMemory`) |
| `20261012T022350` | `5399f034…` (+ 0121) | dxdiag and crash phases ok; `terminate` does not end (below) |
| `20261012T023111` | `5399f034…` | `childabort: done ok`: dxdiag exit 0 and report written, three crash children `c0000005`, workers progressing after each phase |
| `20261012T023809` | `2d99e298…` (0c51652) | `childabort: done ok`, as above |

The report written on the phone (`dxdiag.txt`, 569 bytes) has the System Information section
with `Machine name: PLAYPORT`, the 6 CPUs and 8191 MB, as desktop Wine's has the host's.

## Gates and Mortal Kombat

On `2d99e298…`: Hollow Knight (`app-367520`, run `20261012T023636`) reached its first frame at
+10.2 s and showed its title menu 10 s later; Portal 2 (`app-620`, `20261012T023726`) at +5.8 s,
showing the Source splash. Both `ok`.

Mortal Kombat Komplete Edition (PLA-36) was not played. Its folder is 18 GB; the phone had 11.2 GB
free. House of Golf 2 (`epic-48cfbcf29a4441a894d05a203e03dc74`, 21.9 GB) was uninstalled through
the UI (`20261012T023918`), leaving 33.1 GB, and the owner's copy was put in `Documents/Import`
(1,530 entries, 271 s). The import (`import:Import/Mortal Kombat Komplete Edition`) then stopped
before copying: *Not enough free space: 20.5 GB needed, 15.8 GB free*. The importer clones on the
same volume, which takes no space, but its free-space check counts the whole folder plus its
reserve. Removing more of the owner's titles was not authorised, so the Import copy was deleted and
the paused import cancelled through Downloads (`20261012T024745`: *Nothing downloading*, 35.1 GB
free). House of Golf 2 was not reinstalled. The probe's `dxdiag` phase runs what Mortal Kombat
runs, an i386 process starting `dxdiag.exe /whql:off /t …` while its threads are busy.

## What stays open

- **A terminated thread runs on.** On iOS a thread that wineserver ended stops at its next server
  call or fault. A `TerminateThread`ed worker that only computes never stops; the `terminate`
  phase waits 15 s for each and does not end in a run's time, and the threads of a crashed child
  that make no server call stay alive (and busy) after their process ended. That needs an
  asynchronous abort at a safe point; a follow-up.
- **The abort path of 0121 has not run on the phone.** No probe run aborted a thread holding the
  registry lock (no `[section]` line); what made the probe pass is the `virtual_alloc_teb` order.
- **Mortal Kombat** waits for space: the import's check counts a clone's bytes (above).
- **Code rewritten in place is not translated again.** The probe's workers rewrite
  `mov eax, imm32; ret` in an RWX page, call `FlushInstructionCache` and call it. On the phone
  most calls return the value written before (for example 63,126 of about 72,000 calls in run
  `20261012T023111`); on desktop Wine none do. FEX does not see a write to RWX memory it has
  translated. A follow-up: it matters to titles that patch code in place.
  **Amended 2026-10-12 ([jit-alias-release](2026-10-12-jit-alias-release.md), PLA-201):** still
  87.5 % after wine-pe 0038. Wine's i386 `FlushInstructionCache` made no syscall, and FEX's write
  trap misses a 4 KiB page that shares a 16 KiB host page with writable pages. wine-pe 0039
  makes the flush reach FEX: 0 of 44,818 calls stale on `b1f0278b…`.

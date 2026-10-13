# Evidence: Counter-Strike `-soft`; host-fault storms end the title, not the app (PLA-164)

**Date:** 2026-10-12. **Kind:** phone runs (dev app), log analysis, host tests.
**Linear:** [PLA-164](https://linear.app/playportdev/issue/PLA-164); titles
[PLA-22](https://linear.app/playportdev/issue/PLA-22) (Counter-Strike),
[PLA-25](https://linear.app/playportdev/issue/PLA-25) (Half-Life, the same install folder).
**Changes:** `patches/madeira-unix` 0118 and 0119, `patches/wine-pe` 0037.
The owner was away, so the conservative option was taken at each choice below.
**IPA:** `Playport-26.5-7138d11e.ipa` (dev), sha256
`7138d11e655e14be7a554eb31e336d38215c2bfe993841b42d0733b42b174398`, built from commit 4bec238
and installed in place. Every phone run below except the first is on it. Run directories are under
`$PLAYPORT_BUILD/ui-runs/`.

## Summary

- The earlier reading was wrong. The fault at "a thread's stack end" was neither a stack probe nor a
  small host stack. `x0` and `RSP` (`0x7041a60000`) were arguments and stale emulator state on a
  thread whose process had already ended. There are three faults in a chain, and the first one
  ends the game itself.
- **1. An unaudited thunk ended the game (fixed: wine-pe 0037).** With `-soft`, wined3d's surface
  `GetDC` calls `D3DKMTCreateDCFromMemory`. Its wow64win thunk was not audited for the i386 guest
  window, so the process stopped itself with `c00000bb` 0.6 s after its first frame
  (`i386 win32u system call 10de … its thunk is not audited for the guest window, stopping`).
- **2. A thread of the ended game faulted, and the app went down with it (fixed: madeira-unix
  0118).** A wined3d thread (`0054`) ran on after its process's exit had reclaimed its pool
  copies. It branched into libwow64fex's reclaimed copy. It was then declined into its own SIGBUS
  handler and faulted at `KiUserExceptionDispatcher`'s reclaimed copy. It was redelivered there
  2,000 times, until the redelivery breaker terminated the app.
- **3. After 0037, the game fails its own fallback, and that fault storms (contained: madeira-unix
  0119; cause still open).** The software renderer asks for 640×480 full screen.
  `ChangeDisplaySettings` on `\\.\DISPLAY1` fails (`find_source FAILED → BADPARAM`: the iOS port
  registers no display source). The game says *"The specified video mode is not supported. The game
  will now run in gl mode."* and unloads SDL2.dll. Creating that message box then sends a message to
  SDL's window, whose procedure (`0x7a411fd0`) was in the unloaded DLL. FEX reports
  `NoExec instruction in entry block` and its fault path reads NULL at one pc in a loop. 0119 ends
  that title with `c0000005`, and the app shows *Counter-Strike stopped unexpectedly*.
- Host tests pass: `pp test --quick`, including `build/wow64/dc_from_memory_test.c`. `pp names` and
  `pp secrets` are clean. The gates pass on `7138d11e…`: Hollow Knight, Portal 2 and Counter-Strike
  on OpenGL.

## 1. `D3DKMTCreateDCFromMemory` (wine-pe 0037)

Runs `20261011T081653` and `20261011T084644`, IPA e5a928b era (before this work):

- `002c:err:wow:window_unaudited Playport: i386 win32u system call 10de (args …: 00c0f554 0a1f0030 00000016 00000500); its thunk is not audited for the guest window, stopping`
  (`0x10de` is `NtGdiDdDDICreateDCFromMemory` in `win32syscalls.h`).
- `NtTerminateProcess(handle=0xffffffffffffffff, exit_code=0xc00000bb)`, then
  `abort_process … tearing down this pseudo-process only`, then
  `session: title 1: ended, exit code c00000bb after 636 ms`.

Before the change, the thunk passed `pMemory` (the surface's bits) and `pColorTable` with
`UlongToPtr`. 0037 converts both through the window (`wow64_to_host`; a NULL colour table stays
NULL). The device DC and the DC and bitmap it returns are handles. 0037 audits
`NtGdiDdDDICreateDCFromMemory`, and also `NtGdiDdDDIDestroyDCFromMemory`, which takes two handles.
The host test compiles the patched thunk with the window helpers. It checks the bits and table
through the window, a NULL table, the handles written back, and a NULL descriptor. It also checks
that both names are in `window_audited.h`.

On `7138d11e…` no thunk stops the game: the run gets past the first frame to the mode change (below).

## 2. A thread of an exited process (madeira-unix 0118)

The same runs, after the title ended:

- `[wow64-window] retired base=0x7038950000: 13 secondary thread(s) left; the last one's reclamation tears it down`.
  The process's other threads run on until a server call ends them (madeira-unix 0041).
- Thread `0054`, `teb=0x70b8880000 peb=0x127f70000` (the ended title's PEB): `UNHANDLED #1 pc=0x73fd76937c`.
  That is `libwow64fex.dll+0x11937c`, the original (`r--`) image address. The pool lookup
  (`pc pool-copy pe=0x0 owner=0xffffffffffffffff`) found no copy, because the copies had been
  reclaimed. `lr` was in wow64.dll's pool copy, `blr x8`.
- `BUS #1 … exec=1`, then `[exc-disp] raise`, then `UNHANDLED #3 pc=0x73fdb59380`
  (`KiUserExceptionDispatcher` in the child's ARM64EC ntdll, also reclaimed). Then
  `[mach-deliver] #0…#1900` at the same pc, and
  `[redeliv] 2000 identical redeliveries … — unrecoverable host fault misdelivered to guest`
  and `[redeliv] terminating process`: the app ended.

0118 gives the Mach exception server a check before its last-resort guest delivery: is the
faulting thread registered, and is its PEB a dead child's? `ios_peb_is_dead_child` is now shared;
child PEBs are never unmapped, so a dead PEB is never a live process's. If so, the server diverts
the thread to `abort_thread`, as its next server call would, and logs
`[host-fault] #N thread teb=… of exited process peb=… faulted at pc=… addr=… (lr=…); ending the thread`.
Live processes' faults are unchanged.

On the phone, 0118 did not fire in the runs below: after 0037 the game no longer ends while its
threads run. The case is from the logs above, and was not reproduced on `7138d11e…`.

## 3. The mode change, the unloaded wndproc and the storm (madeira-unix 0119)

On `15f4703e…` (0118 and 0037, before 0119), run `20261012T004746`, `-soft`:

- `[iOS NtUserChangeDisplaySettings ENTRY] devname='\\.\DISPLAY1' … mode: 640x480 @0Hz bpp=16`, then
  `find_source FAILED → BADPARAM`. The same happens with `bpp=32`.
- `MessageBoxIndirectW [msgbox] style 0x31 caption L"Video mode change failure" text L"The specified video mode is not supported.\nThe game will now run in gl mode."`
- `RtlDeleteCriticalSection … crit=7A4EE400` and `[iOS-xrem] via=section … 0x70b2c60000-0x70b2e58000`:
  SDL2.dll (guest `0x7a310000`, `0x1f7000` bytes, host `0x70b2c60000`) is unmapped, and its
  executable interval goes with it.
- `NoExec instruction in entry block: 7A411FD0`, inside SDL2's old `.text`. The backtrace is
  `NtUserCreateWindowEx` → `apply_window_pos` → `window_from_point` → `send_message` →
  `call_window_proc` → `KeUserModeCallback`.
- `UNHANDLED #1 pc=0x128bbc370 addr=0x0` (`ldr x1,[x1]` in pool code), redelivered to the guest
  until `[redeliv] 2000 identical redeliveries`, then `terminating process`: the app ended.

So the game calls a window procedure in a DLL that it unloaded itself. That happens only on its
fallback from a failed mode change. On Windows, and in desktop Wine, the mode change succeeds and
the game reaches `Playing Startup Videos`.

0119: at the breaker's threshold (2,000 identical `(thread, pc, addr)` deliveries, unchanged), a
thread of a live child is diverted to `NtTerminateProcess(NtCurrentProcess(), STATUS_ACCESS_VIOLATION)`
on its own stack. A child is a PEB with a registered identity (`ios_peb_is_child`) that has not
exited. That is how a guest's own unhandled exception ends. Each child gets this once: a second
storm in the same child, and any storm in the session, still end the app. The threshold is
unchanged because a storm below 2,000 has never been shown to be unrecoverable.

### On `7138d11e…`

| Run | Arguments | Result |
|---|---|---|
| `20261012T005554` | `-soft`, `--until first-frame+30 --shot` | first frame +5.17 s, `dialog shown` +5.48 s; `[host-fault] 2000 identical redeliveries pc=0x124ef0370 addr=0x0 on thread teb=0x70b8750000 of process peb=0x126390000; ending the process with c0000005, not the app`, then `abort_process … pseudo-process only`, then `session: title 1: ended, exit code c0000005 after 1203 ms`; the run ends `done exit=0xc0000005`, and the app was alive until the driver ended it (pid 35852) |
| `20261012T010505` | `-soft`, `--leave-running`, then `pp phone shot` at +8 s and +68 s | same end (`done exit=0xc0000005`). At +68 s the Home screen shows the alert **"Counter-Strike stopped unexpectedly. It exited with code 0xC0000005."** with OK (`cs-soft-after-68s.png` in the run directory). The app was alive at +70 s (`pp phone status`, pid 35982), then `pp phone kill` |
| `20261012T010728` | none (the game's saved renderer: software) | same chain and the same clean end, `exit=0xc0000005` |
| `20261012T011805` | `-soft -windowed`, then `tap`, `wait:40` | no mode change and no dialog. `Playing Startup Videos...` (desktop parity). The game runs for 90 s with its main thread busy, but the screen stays black: its frames go out as GDI `put-image` row blits (`1280x1`, bottom-up, about 10 million in 90 s), and nothing shows them. That is the GDI-window path (PLA-152), not this issue |
| `20261012T012032`, `20261012T012134`, `20261012T012347` | `-gl` / `-gl +map de_dust2` | first frame, then black. The main thread waits in a host mutex (`__psynch_mutexwait`) after `STEAM Auth Server` (with `+map`, after `serverbrowser.dll`; without, after `Playing Startup Videos`). The game kept windowed mode from the `-windowed` run. Not seen in full screen; follow-up |
| `20261012T012507` | `-gl -full +map de_dust2`, `--until first-frame+30 --shot` | **ok**: `couldn't exec maps/de_dust2_load.cfg`, first frame +4.92 s; the screenshot shows de_dust2 (stone arches, crates, the buy-zone cart). This is the Counter-Strike default-renderer gate |

## Gates on `7138d11e…`

- Hollow Knight (`app-367520`), `20261012T010812`: first frame +9.67 s, ok at first frame +10; the
  screenshot shows the title menu.
- Portal 2 (`app-620`), `20261012T010912`: first frame +5.74 s, ok at first frame +10.
- Counter-Strike on OpenGL (Zink), `20261012T012507`: above.

## Open

- Display modes: the iOS port registers no display source, so a named `ChangeDisplaySettings`
  fails (`BADPARAM`), while an empty name is answered `SUCCESSFUL`. GoldSrc's software renderer, and
  any game that changes mode on a named device, takes its failure path. The next step is to give
  the primary display a source with the modes games ask for, and present them scaled, before
  `-soft` can run in full screen. Follow-up issue.
- `-soft -windowed` runs, but nothing shows its GDI frames (PLA-152's area).
- Windowed GL Counter-Strike waits in a host mutex after `serverbrowser.dll` (a follow-up).
  `-full` puts the game back in full screen.
- 0118 has not fired on the phone (the trigger is gone after 0037). Its case is the logs in §2.
- 0119 does not cover other unhandled host faults: native-code faults declined to a SIGBUS that
  wine cannot deliver, and storms in the session.

# Evidence: JWE's stall was a killed sound-engine thread, not a lost I/O completion (PLA-238)

**Date:** 2026-10-12. **Kind:** phone runs (dev app) and a read of the runtime's code. **Linear:**
[PLA-238](https://linear.app/playportdev/issue/PLA-238), follow-up of
[PLA-41](https://linear.app/playportdev/issue/PLA-41) (Jurassic World Evolution, Epic
`epic-373a4372c20540aca7fdd880d27fa49a`). The owner was away; the conservative option was taken at
each choice. Run directories are under `$PLAYPORT_BUILD/ui-runs/`. Every JWE run was on Vulkan for
the session only (decision 0100).

**IPAs (dev):** `895abae8d4df13e7678aea400114726655fe2c57c391e125d1def67ce984a85b` (4d93ca2, the
fastsync A/B), `4be4d8a709006b69f48a1e6ca7c8c0cc4c423971606c2c7d8ccde2d1a7505f2a` (wine-unix 0035,
the I/O log), `98fb322385a3975729d79aa40c38226985a4f29a850943dea5d2de0ac7af5c14` (0035 and
madeira-unix 0132; installed at the end).

## Where 2.11c left it

Main, Render and the `fTask` workers waited on unsignaled auto events in the engine's job loop, the
`fIO 0`–`7` pool waited in `NtRemoveIoCompletion`, and at about 47 s the `WatchDog` crashed the game
on purpose ([jwe-watchdog](2026-10-12-jwe-watchdog.md)).

## The I/O and event log (wine-unix 0035)

`patches/wine-unix` 0035 is a dev diagnostic, off unless `WINEDEBUG` names `warn+iolog` (set for one
session through the title's launch setting `runtime: env.WINEDEBUG=warn+waitlog,warn+iolog`). It
logs each completion-port binding and notification mode (with the file's name), each read or write
on an overlapped handle (the immediate status, what `NtReadFile`/`NtWriteFile` returns, the event,
APC and completion value, and whether a packet was queued under the server's rule, including
`FILE_SKIP_COMPLETION_PORT_ON_SUCCESS`), each packet `NtRemoveIoCompletion`/`Ex` returns, each
`NtSetIoCompletion`, and `NtSetEvent`/`NtResetEvent`/`NtPulseEvent` with their callers on any object
a thread waits on with no timeout. After 3 s without such an operation, the next wait or delay
dumps the last operations on each of those objects, the bound files and the last packets. It also
makes the iOS `NtWaitForSingleObject` the wait log's wrapper: 0033 had given the wrapper the body's
name, so single-object waits were never logged. Off, it costs one debug-channel test per wait,
delay, event operation, overlapped read or write and completion removal.

## Runs

| Run | IPA | Session setting | Outcome |
| --- | --- | --- | --- |
| `s11f-jwe-fsoff` | `895abae8` | `set:dev.fastsyncOff=true` (`MADEIRA_FASTSYNC=0`), `warn+waitlog` | `exit=0xc0000005` after 48 s, as with fastsync on |
| `s11f-jwe-iolog` | `4be4d8a7` | `warn+waitlog,warn+iolog` | the same after 48 s; the log below |
| `s11f-jwe-fix` | `98fb3223` | none | no WatchDog; 300 % CPU to the end; `exit=0xc000001d` after 44 s |
| `s11f-jwe-fix2` | `98fb3223` | none | the same, at the same guest RIP |
| `s11f-hk` | `98fb3223` | none | Hollow Knight, first frame +10.09 s, its main menu |
| `s11f-p2` | `98fb3223` | none | Portal 2, first frame +5.81 s |

## Findings

**Fastsync is not involved.** With it off for the session the run ends the same way.

**The I/O completion path is clean.** In `s11f-jwe-iolog` the engine has one port (`0xbc`, key 0).
The jobs post requests to it (`NtSetIoCompletion` from `JWE.exe+0xa2967f`), an `fIO` thread takes
each, opens and binds the file (`Win64\ovldata\…\*.ovl`, the Wwise `audio\init` bank), and reads it
overlapped. Every read returned `STATUS_PENDING` after a synchronous success, queued its packet
(no file sets a notification mode), and another `fIO` thread removed that packet with the right
byte count (15 read packets for 15 queued reads, beside 106 posted requests; none lost). Reads with the event's low bit set queued none, as Windows
does. No read hit end of file or failed.

**The last signals are the sound engine's.** The dump after 3 s of quiet shows the last operations
at +8.66 s: Wwise's I/O thread (`009c`) posts a read of the init bank, `fIO` reads it in three
pieces (32 KiB, 32 KiB, 3365 bytes: the whole file), sets the bank thread's event (`0x288`), and the
bank thread (`00a4`) passes it on (`0x1b4`). Then nothing operates on any watched event again.

**The runtime killed the bank thread.** About 1 s later the Mach exception handler's livelock
breaker (`signal_arm64_ios.c`, ml385/ml876) logs `[fault-stuck] BREAKING LOOP: page … pc
0x11f4b6000 repeated 50 times -> diverting thread to abort_thread`, and the server ends `00a4`. The
faults it counted are alignment faults (Windows code `80000002`; the handler's `[fault_rip]` lines
show such faults arrive with `kr=257`, `EXC_ARM_DA_ALIGN`): a misaligned atomic
(`f8e380e3`) on a stack slot that crosses a 16-byte boundary, a guest split lock. Each one goes on to
the SIGBUS path, where FEX emulates it and resumes after it (`Handled unaligned atomic: new pc:
11F4B6004`). The thread was running a loop, not stuck. Every JWE run since the game got past its
anti-tamper ends the same way (`s11-jwe-base`, `s11-jwe-vk`, `s11c-jwe-wait*`, `s11f-jwe-*`): one
Wwise thread killed at about +9.6 s. With the sound engine's bank thread gone, its start never
finishes, nothing feeds the job loop, and the WatchDog fires.

## The fix (madeira-unix 0132)

The breaker no longer counts alignment faults, and they leave its state as it was. A misaligned
access FEX cannot complete becomes the guest's exception, not a loop, so the breaker loses nothing
it was for. On `98fb3223` no thread is killed: the bank thread finishes, and `Main` goes on to the
game's protected code at full load (about 300 % CPU from +9 s to the end, against 74 % idle before).

## The next failure

Both runs on `98fb3223` end `exit=0xc000001d` after 44 s, at the same place: on `Main`, FEX finds an
invalid instruction at guest RIP `0x142d74711` (`Invalid instruction in entry block`), in the
packed `.ecode` section, and the exception is unhandled. Before it `Main` ran about 380,000
misaligned atomics on the SIGBUS path (about 14,000 a second, each a Mach exception, a signal, a
Windows exception preparation and three log lines) and about 143,000 one-page `NtProtectVirtualMemory`
calls. The section's bytes there on disk are not valid code (`ff ff ff 21 …`), so the code the
protection should have written there was not written, or the protection jumped there on purpose.
Which of the two, and whether the time its split locks take matters, is open. No frame is presented.

## Next

1. Find why the protection reaches `0x142d74711`: compare the bytes there at the fault with the
   file's, and the protects of its page (a dev-only log of who writes that page last). A desktop
   Proton run would show whether Windows timing matters, but needs an Epic launch token on the
   workstation.
2. Measure the cost of one misaligned atomic on the SIGBUS path and what the `[rtcs]` and FEX debug
   lines add to it; a faster path (no per-fault log lines, or FEX's own handling before the
   Windows exception) would help every title with guest split locks.

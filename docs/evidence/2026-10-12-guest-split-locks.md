# Evidence: what a guest split lock costs, and a cheaper path that keeps its semantics (PLA-242)

**Date:** 2026-10-12. **Kind:** a probe on desktop Wine and on the phone (dev app), a read of the
runtime's fault path, phone runs. **Linear:** [PLA-242](https://linear.app/playportdev/issue/PLA-242),
from [PLA-238](https://linear.app/playportdev/issue/PLA-238); JWE is
[PLA-41](https://linear.app/playportdev/issue/PLA-41), its `c000001d` is
[PLA-241](https://linear.app/playportdev/issue/PLA-241). The owner was away; the conservative option
was taken at each choice. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

**IPAs (dev):** before, `98fb322385a3975729d79aa40c38226985a4f29a850943dea5d2de0ac7af5c14` (4b4a66b);
after, `fc335c10b85d6e45e7739e66cd126bab6a86fe6cdde955b1e329b1d986560945` (wine-pe 0040, fex 0032,
madeira-unix 0133; installed at the end).

## The path

A locked x86 operation on a slot that crosses a 16-byte boundary is a misaligned ARM64 atomic under
FEX. The phone has LSE2, so a misaligned atomic inside 16 bytes runs natively. One that crosses 16
bytes raises an alignment fault (`EXC_ARM_DA_ALIGN`, esr `0x92000021`):

- `lock cmpxchg` is a `CASAL`. The Mach exception handler emulates it itself, on its server thread
  (`signal_arm64_ios.c` ml431, `mach_vm_read`/`mach_vm_write`), and resumes the thread.
- `lock xadd` (`LDADDAL`), `xchg` (`SWPAL`) and the other LSE forms are declined to the BSD signal.
  `bus_handler` runs `virtual_handle_fault`, reads the target with `mach_vm_read_overwrite`, asks
  for the page's host protection (`mach_vm_region`), decodes the instruction as a plain access
  (refused), and raises `80000002`. The PE side builds the Windows exception
  (`prepare_exception_arm64ec`), FEX's `ResetToConsistentState` runs `HandleUnalignedAccess` (two
  64-bit CAS loops for an access across 16 bytes) and returns through `NtContinueNative`.
- Each one logged three lines on x86-64: `[rtcs] pre` (its 40-line cap closes before the line),
  FEX's `Exception: Code: 80000002` and `Handled unaligned atomic`. The WOW64 module logs one.

FEX on Linux takes the same handler from its SIGBUS handler, one signal round trip. Its other remedy,
unaligned-atomic backpatching, is a Linux kernel feature (`KernelUnalignedAtomicBackpatching`, the
kernel's handler); FEX's user-space backpatcher only turns unaligned `LDAR`/`STLR` into plain
accesses with a barrier, which the Mach handler already does here. Neither can make a split RMW
atomic, so there is no split-safe code for FEX to emit at a hot site.

## The probe

`tools/compat/probes/splitlock.c`, built for x86-64 (`splitlock.exe`, 4- and 8-byte slots) and i386
(`splitlock-i386.exe`, 4-byte), put in `Documents/Import/splitlock` and `…/splitlock-i386` with AFC
and imported through the UI (`import:Import/splitlock`, titles `dir-splitlock`, `dir-splitlock-i386`).
For each slot it runs `lock xadd`, `lock cmpxchg` (each must succeed, then one wrong expected value
must fail) and `xchg`, checks every returned value, the slot's final value and the bytes around it,
and times the loop. Then four threads contend on a 4-byte slot across 16 bytes.

Nanoseconds per operation, single-threaded (aligned and inside-16 controls: 0.6 to 3 ns on the
phone, 3.6 ns on desktop Wine):

| Slot | Op | desktop Wine 64 | phone 64, before | phone 64, after | phone i386, before | phone i386, after |
| --- | --- | --- | --- | --- | --- | --- |
| 4 B across 16 | xadd | 3.6 | 75,299 | 46,752 | 56,798 | 48,856 |
| 4 B across 16 | cmpxchg | 3.5 | 14,131 | 14,023 | 12,906 | 13,016 |
| 4 B across 16 | xchg | 3.6 | 75,612 | 48,089 | 51,918 | 44,248 |
| 4 B across 64 | xadd | 570 | 65,954 | 35,438 | 42,146 | 38,983 |
| 4 B across 64 | xchg | 561 | 61,325 | 35,831 | 42,282 | 37,164 |
| 8 B across 16 | xadd | 3.6 | 67,970 | 36,248 | | |
| 8 B across 16 | xchg | 3.6 | 60,811 | 36,959 | | |
| 8 B across 64 | xadd | 567 | 61,224 | 38,976 | | |
| 8 B across 64 | xchg | 560 | 61,378 | 38,205 | | |
| 8 B across 64 | cmpxchg | 755 | 13,447 | 18,112 | | |

Every single-threaded check was `ok` on every run (`done ok`). Contended, four threads × 1000:
`lock cmpxchg` and an `xchg` spinlock around a plain counter lost nothing; `lock xadd` lost 2 to 7
of 4000 on the phone, before and after (desktop Wine: none). That is FEX's handler: across 16 bytes
it is two CAS loops, and when the second fails after the first stored, it returns without retrying
("CAS managed to tear", as AMD tears). The Mach handler's CAS emulation does not tear among split
locks because one thread runs them all, but it is not atomic against another thread's aligned access
to the same bytes.

Runs: `s11g-probe-before`, `s11g-probe32-before` (`98fb3223`), `s11g-probe-after`,
`s11g-probe32-after` (`fc335c10`).

## The change

Choice: keep FEX's handler and every decision on the path as they are, and remove what is not the
work. All three are log or diagnostic cuts; no handler changes what it does.

- **wine-pe 0040:** `prepare_exception_arm64ec` counts misalignments and logs the first 8 and every
  65536th with the count. Other codes are logged as before.
- **fex 0032:** ARM64EC and WOW64 count the completed split locks and log the first 8 and every
  65536th with the count. The ARM64EC `Exception:` line for a misalignment is deferred and printed
  only when FEX does not complete it.
- **madeira-unix 0133:** `bus_handler` asks for the page's host protection only for a write; only a
  write can find its write stripped, and an alignment fault reports WnR=0. Its sampled `[wr-strip]`
  line shows `host_prot=-1` for a read.

The x86-64 FEX path went from 61 to 75 µs an operation to 35 to 48 µs; i386, which logged one line,
from 42 to 57 µs to 37 to 49 µs. The Mach handler's CAS path (13 to 18 µs) is unchanged.

Not done, because none keeps the semantics exactly or is contained:

- Emulating the LSE forms in the Mach handler, as it does `CASAL`, would be about 14 µs but is
  `mach_vm_read`/`mach_vm_write` from another thread: not atomic against an aligned atomic on the
  same bytes, where FEX's CAS loops are.
- Delivering `80000002` from the Mach handler would skip the signal, but its identical-redelivery
  terminator (`[redeliv]`, 2000 of the same pc and address) would end JWE's loop, the PLA-238 kill
  again, and `bus_handler`'s guard-page and readability decisions would have to move with it.
- Calling FEX's handler from `bus_handler` crosses into PE code from a signal.

## JWE

`s11g-jwe` (`fc335c10`, Vulkan for the session, no `WINEDEBUG`): `exit=0xc000001d` after 32 s, against
44 s on `98fb3223` (`s11f-jwe-fix2`). It is the same failure at the same place: on `Main`, "Invalid
instruction in entry block: 142D74711", unhandled. Before it, `Main` ran between 458,752 and 524,287
split locks (the last summary line; `s11f-jwe-fix2` logged 496,416) and 154,506 protect syncs
(153,000). No frame. The log fell from 196 MB to 67 MB. The protected code reaches the same RIP after
the same work, 12 s sooner, so the time its split locks take does not decide this exit.

## Gates on `fc335c10`

| Run | Title | Result |
| --- | --- | --- |
| `s11g-hk` | Hollow Knight (`app-367520`) | first frame +9.00 s, its main menu |
| `s11g-p2` | Portal 2 (`app-620`) | first frame +5.89 s |

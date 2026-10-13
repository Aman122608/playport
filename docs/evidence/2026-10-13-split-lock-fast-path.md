# Evidence: a guest split lock without its alignment fault, under one lock (PLA-244)

**Date:** 2026-10-12/13. **Kind:** a change to FEX's JIT on an iOS host, the `splitlock` probe on
the phone (dev app), phone runs. **Linear:** [PLA-244](https://linear.app/playportdev/issue/PLA-244),
from [PLA-242](https://linear.app/playportdev/issue/PLA-242) ([guest split locks](2026-10-12-guest-split-locks.md))
and [PLA-243](https://linear.app/playportdev/issue/PLA-243) ([split-lock tear](2026-10-12-split-lock-tear.md));
JWE is [PLA-41](https://linear.app/playportdev/issue/PLA-41). The owner was away; the conservative
option was taken at each choice. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

**IPAs (dev):** before, `8cdd97f2923dc42cd9d5d2304eb4dc0ae6a0c09d4e9f242b4277914d994154e6` (e4a9a5d);
after, `bc3f5b63e53b4f43d97a17ee6044bee2d640adcfcf217683545d803b6f4aae07` (fex 0036; installed at
the end).

## What it cost, and the gap

A locked x86 operation on a slot that crosses 16 bytes is an ARM64 atomic that crosses 16 bytes, and
the phone raises an alignment fault for it. Before this change:

- `lock xadd`, `xchg` and the other LSE forms went from the Mach exception to the BSD signal, the
  Windows exception, FEX's `HandleUnalignedAccess` (under `StrictSplitLockMutex`, fex 0034) and back
  through `NtContinue`: 35 to 50 µs an operation.
- `lock cmpxchg` (`CASAL`) was emulated by the Mach exception handler on its server thread
  (`mach_vm_read`/`mach_vm_write`), 13 to 18 µs, outside `StrictSplitLockMutex`. A `cmpxchg` and an
  `xadd` racing on the same split slot were therefore not serialised against each other.

## The change: fex 0036

On an iOS host build the JIT checks the address of each 16-, 32- and 64-bit `CASAL` and LSE atomic
memory op (`LDADDAL`, `LDCLRAL`, `LDSETAL`, `LDEORAL`, `SWPAL`) before issuing it: `and`, `sub`,
`tbnz`, which change no flags. One inside 16 bytes runs as before. One that crosses 16 bytes calls a
dispatcher stub instead. The stub saves x0–x30, sp, q0–q31 and NZCV, and calls
`HandleSplitAtomicFromJIT`, which decodes the instruction it skipped and completes it with the
same helpers the fault path uses (`HandleCASAL`, `HandleAtomicMemOp`), under the same
`StrictSplitLockMutex`. The stub then writes the result register back and returns past the
instruction. Every split `CASAL` and atomic memory op the JIT issues now takes that one lock, in
both modules (ARM64EC for x86-64, WOW64 for i386). `CASPAL` (`cmpxchg8b`/`cmpxchg16b`) keeps the
fault path. The first 8 and every 65536th are logged (`JIT split lock #N`).

## The probe

`tools/compat/probes/splitlock.c`, with a new contended phase, "mixed": two threads of `lock xadd`
against two of the `lock cmpxchg` increment loop on the same 4-byte slot across 16 bytes. It was
imported as `Import/splitlock-11m` and `Import/splitlock-11m-i386` (titles `dir-splitlock-11m`,
`dir-splitlock-11m-i386`, run `s11m-import`).

Nanoseconds per operation, single-threaded, 5000 operations each. Every check was `ok` on every run:
each returned value, the slot's final value and the bytes around it.

| Slot / op | x86-64 before | x86-64 after | i386 before | i386 after |
| --- | --- | --- | --- | --- |
| 4 B across 16, xadd | 49,900 | 34.8 | 50,213 | 32.8 |
| 4 B across 16, xchg | 48,834 | 16.9 | 46,250 | 15.8 |
| 4 B across 16, cmpxchg | 13,991 | 15.8 | 14,033 | 14.8 |
| 4 B across 64, xadd | 39,740 | 16.5 | 37,311 | 16.0 |
| 4 B across 64, xchg | 39,619 | 16.7 | 34,148 | 15.8 |
| 4 B across 64, cmpxchg | 13,905 | 15.5 | 13,986 | 14.5 |
| 8 B across 16, xadd / xchg | 39,581 / 39,516 | 17.7 / 17.3 | | |
| 8 B across 16, cmpxchg | 13,886 | 15.6 | | |
| 8 B across 64, xadd / xchg | 43,333 / 34,815 | 17.3 / 17.3 | | |
| 8 B across 64, cmpxchg | 14,450 | 15.9 | | |
| aligned, xadd / cmpxchg / xchg (200,000 ops) | 1.7 / 2.0 / 0.6 | 1.9 / 2.3 / 0.8 | 1.8 / 2.8 / 0.8 | 1.8 / 2.9 / 1.0 |

The 34.8 and 32.8 ns of the first split phase include its first translation. The aligned rows are
the cost of the check on every atomic: 0 to 0.4 ns.

Contended: four threads × 1000 on a 4-byte slot across 16 bytes. The table gives the count of 4000
and ns/op.

| Contended | x86-64 before | x86-64 after | i386 before | i386 after |
| --- | --- | --- | --- | --- |
| `lock xadd` | 4000, 22,119 | 4000, 812 | 4000, 21,115 | 4000, 541 |
| `lock cmpxchg` loop | 4000, 19,139 | 4000, 2,275 | 4000, 18,539 | 4000, 1,600 |
| `xchg` spinlock | 4000, 111,748 | 4000, 1,708 | 4000, 105,590 | 4000, 1,722 |
| mixed `xadd` + `cmpxchg` | **3981 (torn)**, 19,665 | **4000**, 884 | **3961 (torn)**, 22,375 | **4000**, 539 |

Before, the runs ended `done ok (contended: torn)`: the mixed phase lost 19 and 39 updates, the gap
recorded in the tear evidence. After, they end `done ok (contended: ok)`. A split lock now costs
15 to 18 ns single-threaded, against 35 to 50 µs (`xadd`, `xchg`) and 14 µs (`cmpxchg`) before:
about 2,500 and 900 times less. Under contention it costs 0.5 to 2.3 µs, against 19 to 22 µs.

Runs: `s11m-probe-before`, `s11m-probe32-before` (`8cdd97f2`); `s11m-probe-after`,
`s11m-probe32-after` (`bc3f5b63`). The after logs show `JIT split lock #1`…`#8` and `#65536` and no
`Unhandled JIT split lock`.

## JWE

`s11m-jwe` (`bc3f5b63`, Vulkan for the session, `--until first-frame+60 --wait 90`): its first window
at +10.46 s was the protection's refusal, "Sorry, something went wrong", with
`…/jpe_f884fd95kfs/?e=88500006`. This is the same online re-validation refusal as `s11l-jwe` on
2026-10-12. The protected job did not run, and no split lock was logged. The driver ended the title
when `--wait` ran out (102 s). The run was not retried. Whether the faster path lets JWE's protected
job beat its WatchDog (74 s on `50ab8577`) is not shown yet.

## Gates on `bc3f5b63`

| Run | Title | Result |
| --- | --- | --- |
| `s11m-hk` | Hollow Knight (`app-367520`) | first frame +9.98 s; the screenshot shows its main menu |
| `s11m-p2` | Portal 2 (`app-620`) | first frame +5.95 s |

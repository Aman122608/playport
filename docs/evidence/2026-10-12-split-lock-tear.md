# Evidence: a contended split `lock xadd` no longer loses updates (PLA-243)

**Date:** 2026-10-12. **Kind:** a read of FEX's unaligned-atomic handler, the `splitlock` probe on
the phone (dev app), phone runs. **Linear:** [PLA-243](https://linear.app/playportdev/issue/PLA-243),
from [PLA-242](https://linear.app/playportdev/issue/PLA-242)
([guest split locks](2026-10-12-guest-split-locks.md)); JWE is
[PLA-41](https://linear.app/playportdev/issue/PLA-41). Owner decision 2026-10-12: correctness first,
on by default even if split locks get slower, with the cost recorded. The owner was away; the
conservative option was taken at each choice. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

**IPAs (dev):** before, `adf030793b0960cfbcfd8a61f6f4a9c3daf7829e982ee169f66c39e60be56688` (c13a442);
after, `50ab857755f116679e8da274f4a42abe8bf5de5498142874bd6834312bde974b` (fex 0034; installed at
the end).

## The handler

A locked x86 operation on a slot that crosses 16 bytes faults on the phone, and `xadd`/`xchg`
(`LDADDAL`/`SWPAL`) reach FEX's `HandleUnalignedAccess` (`FEXCore/Source/Utils/ArchHelpers/Arm64.cpp`)
through the Windows exception, on x86-64 (ARM64EC module) and i386 (WOW64 module) alike. Its
`DoCAS32`/`DoCAS64` path across 16 bytes is two CAS loops, upper half then lower half. When the
lower CAS fails after the upper one stored, the access has torn ("CAS managed to tear"); an atomic
memory op then returns without its update, and the other thread's update to the lower half is mixed
with this one's upper half. `lock cmpxchg` (`CASAL`) never reaches FEX here: the Mach exception
handler emulates it on its one server thread, so those do not tear among themselves.

`StrictInProcessSplitLocks` passes `&CTX->StrictSplitLockMutex` into the handler, and every split
path in it (`DoCAS16/32/64` across 16 bytes or a cache line, `RunCASPAL`) takes that one
process-wide spin lock (WFE wait) around its CAS loops. With it, no two split accesses the handler
completes interleave, so the two CASes only fail against a plain or aligned write by another thread
to the same bytes. It changes no code generation (`AffectsCodeGen: false`); its other use, the
Linux kernel's unaligned-atomic control, returns at once on iOS (`SetKernelUnalignedAtomicControl`).

Choice: (b), default the option on for iOS, not (a), a retry in the handler. A retry after a tear
cannot restore atomicity, since the upper half is already stored and undoing it is another CAS that
can fail; serialising is the upstream remedy and needed no handler change.

## A/B before the change (session setting)

On `adf03079`, the session setting `runtime: env.FEX_STRICTINPROCESSSPLITLOCKS=1` (the log shows
`environment FEX_STRICTINPROCESSSPLITLOCKS=1`) made the contended `lock xadd` lose nothing on both
probes (`s11i-probe-strictenv`, `s11i-probe32-strictenv`), against 8 and 5 of 4000 lost without it
(`s11i-probe-before`, `s11i-probe32-before`).

## The change: fex 0034

`StrictInProcessSplitLocks` defaults to true in a build with `FEX_IOS_HOST_BUILD` (both the ARM64EC
and the WOW64 module); `Config.json.in` takes the default from CMake, and other builds keep false.
`FEX_STRICTINPROCESSSPLITLOCKS=0` still turns it off for a session. The `FEX: TSO config` line now
reports `strictsplitlocks=`; every run on `50ab8577` logs `strictsplitlocks=true`, in both modules.

## The probe

`tools/compat/probes/splitlock.c`, titles `dir-splitlock` (x86-64) and `dir-splitlock-i386`, as in
PLA-242. Contended: four threads × 1000 on a 4-byte slot across 16 bytes.

| Contended, across 16 | x86-64 before | x86-64 strict env | x86-64 after | x86-64 after (2nd) | i386 before | i386 strict env | i386 after |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `lock xadd`, counted of 4000 | 3992 (torn) | 4000 | 4000 | 4000 | 3995 (torn) | 4000 | 4000 |
| `lock xadd`, ns/op | 22,797 | 23,071 | 25,056 | 23,796 | 22,386 | 23,681 | 24,154 |
| `lock cmpxchg`, ns/op (4000 of 4000) | 18,628 | 22,678 | 20,740 | 20,674 | 19,784 | 20,539 | 21,210 |
| `xchg` spinlock, ns/op (4000 of 4000) | 112,505 | 118,309 | 118,307 | 119,592 | 111,207 | 120,934 | 119,006 |

Single-threaded, ns/op (every check `ok` on every run; `done ok`):

| Slot / op | x86-64 before | x86-64 strict env | x86-64 after | i386 before | i386 strict env | i386 after |
| --- | --- | --- | --- | --- | --- | --- |
| 4 B across 16, xadd | 47,147 | 87,317 | 47,646 | 53,379 | 98,884 | 54,835 |
| 4 B across 16, xchg | 48,425 | 46,970 | 45,843 | 44,700 | 44,915 | 45,852 |
| 4 B across 64, xadd | 39,510 | 35,671 | 36,009 | 35,611 | 35,282 | 35,714 |
| 4 B across 64, xchg | 37,776 | 36,524 | 36,613 | 35,093 | 36,085 | 36,672 |
| 8 B across 16, xadd | 35,452 | 36,139 | 36,767 | | | |
| 8 B across 16, xchg | 36,526 | 39,278 | 37,065 | | | |
| 8 B across 64, xadd | 35,615 | 41,941 | 37,856 | | | |
| 8 B across 64, xchg | 39,282 | 36,952 | 37,613 | | | |
| any across, cmpxchg | 12,784–16,503 | 12,814–14,090 | 12,623–17,753 | 13,504–13,863 | 13,205–13,910 | 12,998–13,246 |

The cost: single-threaded, none measurable (an uncontended spin-lock acquire beside a 35–55 µs
fault round trip); the 87 and 99 µs `xadd` across 16 in the strict-env runs are their first split
phase and did not recur in the builds with the default (48 and 55 µs, `s11i-probe-after2` 49 µs).
Contended, four threads on one slot: `lock xadd` 4–8 % slower (22.4–22.8 µs → 23.8–25.1 µs),
because the handlers now wait for each other. `cmpxchg` does not take the lock; its spread is run
to run. One or two samples each.

Runs: `s11i-probe-before`, `s11i-probe32-before`, `s11i-probe-strictenv`, `s11i-probe32-strictenv`
(`adf03079`); `s11i-probe-after`, `s11i-probe32-after`, `s11i-probe-after2` (`50ab8577`): all
`done ok (contended: ok)` after, `(contended: torn)` before.

## JWE

`s11i-jwe` (`50ab8577`, Vulkan for the session, `--until first-frame+30`): first frame at +38.72 s
(black, as before), and the title ended `exit=0xc0000005 after_s=75` during the stop: its WatchDog's
deliberate write at `0x140a6c820` (`AV WRITE of 1`), as in `s11h-jwe-long2` (first frame +37 s,
WatchDog at 74 s). It had completed more than 1,310,720 split locks (the last sampled line) by
then, as before. The tear did not cause this end; split-lock time (PLA-244) remains the lead.

## Gates on `50ab8577`

| Run | Title | Result |
| --- | --- | --- |
| `s11i-hk` | Hollow Knight (`app-367520`) | first frame +9.06 s, its main menu |
| `s11i-p2` | Portal 2 (`app-620`) | first frame +6.03 s |

## Not covered

The lock serialises only the split accesses FEX's handler completes. A `lock cmpxchg` across 16
bytes, emulated in the Mach handler, does not take it, so a `cmpxchg` and an `xadd` racing on the
same split slot can still interleave; so can a split access and another thread's aligned write to
the same bytes. The probe tests neither, and neither is known in a title.

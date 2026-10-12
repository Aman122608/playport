# Evidence: one-page NtProtect cost, Football Manager 2022's user shared data read, and the anti-tamper wild store (PLA-118, PLA-122)

**Date:** 2026-10-12. **Kind:** phone runs (dev app) with a probe import, log analysis, a desktop
Wine run, host tests. **Linear:** [PLA-118](https://linear.app/playportdev/issue/PLA-118) (one-page
protects), [PLA-122](https://linear.app/playportdev/issue/PLA-122) (the wild store), titles
[PLA-114](https://linear.app/playportdev/issue/PLA-114) (Football Manager 2022) and
[PLA-41](https://linear.app/playportdev/issue/PLA-41) (Jurassic World Evolution).
**Changes:** `patches/madeira-unix` 0122 (LRCPC2 loads and stores in a sub-floor window) and 0123
(diagnostics), the probe `tools/compat/probes/protect.c`, the test
`tools/tests/test_subfloor_rcpc2.py`. The owner was away, so the conservative option was taken at
each choice below. **IPA:** `Playport-26.5-57d12a62.ipa` (dev), sha256
`57d12a6211b158b6da092665439332d1c34b68bbe770d5d35c960f658eb2ef35`, built from commit 2b1501e and
installed in place; the gates and the last runs are on it. Run directories are under
`$PLAYPORT_BUILD/agent-notes/speedrun/ntprotect/`.

## Summary

- **The one-page protects were not what stopped Football Manager 2022.** In the 2026-10-09 run
  (`fm22ra/fm22-hang`, IPA `62b62a7c…`) all 21,400 of `fm.exe`'s one-page `PAGE_EXECUTE_READ`
  protects fall between 00:36:58.5 and 00:36:59.7 (the `[xp]` timestamps), so about 1.2 s. On
  `3e81f622…` (`fm22-base`) the 25,537 image protects took 2.3 s, about 90 µs each with the
  anti-tamper's own work between them. `fm.exe` has no pool copy, so the sync's scan (61 JIT
  mappings, two compares each) never matches and copies nothing.
- **What did stop it** was a fault that never ended. From about a second after start the main
  thread sat at one host instruction of a FEX block (guest `0x14ea86d40`) while the Mach exception
  thread `wine-x18-exc` used 71% of a core (Mach messages 10,129 → 29,816 between two watchdog
  reports), for the whole 6 or 15 minutes of each run. The log names it once:

  ```text
  [subfloor] ml939 via=mach UNHANDLED encoding 0x994003a6 at pc=0x120c31784 for guest addr 0x7ffe000c (real 0x703800000c) -- add it to ios_emulate_load/store
  ```

  `0x994003a6` is `LDAPUR w6, [x29]`: with `FEX_HOSTFEATURES enablelrcpc2` FEX lowers a guest load
  at an offset to LDAPUR (FEAT_LRCPC2). The anti-tamper reads `KUSER_SHARED_DATA`'s interrupt time
  (`0x7ffe000c`, `High1Time`) from x86 code. Playport keeps the user shared data elsewhere
  (`0x7038000000`), and the sub-floor service emulates each guest access to `0x7ffe0000`. It knew
  LDAPR and LDUR but not LDAPUR. It declined, the Mach handler resumed the thread at the same
  instruction, and the thread faulted again.
- **The fix (madeira-unix 0122):** decode FEAT_LRCPC2's unscaled class once
  (`ios_rcpc2_unscaled`), serve LDAPUR and LDAPURS* as LDUR's loads and STLUR as a release store,
  and classify both for the backing probe. None of these forms writes back.
- **Football Manager 2022 now gets as far as Jurassic World Evolution.** It decrypts its image
  (37,259 protects) and ends `c0000005` 4 to 5 s in, on a store to a computed address that no
  allocation covers, at the same guest RIP every time (`0x159782e84`,
  `mov [r10-0x5df0e8b6], rax`, address = `r10 + rdx`). Jurassic World Evolution does the same
  at `0x1488a3cf6`, as on 2026-10-09.
- **The register state on the unaligned-atomic round trip is not the cause (madeira-unix 0123).**
  Both titles run x86 `xchg` on unaligned stack slots, and FEX completes each SWPAL on the SIGBUS
  path. bus_handler now records the faulting registers, and `signal_set_full_context` logs what
  differs in the state the thread resumes with. For all 7 of Football Manager 2022's resumes and
  the first 32 of Jurassic World Evolution's 4,420 (the log's cap), only the atomic's Rt changed. CPSR, SP,
  LR, FP, x18 and all 32 vector registers came back unchanged.
- **Neither is it FEX's TSO lowering.** Football Manager 2022 with TSO off
  (`"ordering":{"tso":false}`) dies at the same RIP (store to `0x70c4608529`). On 2026-10-09
  `FEX_SMCCHECKS=full` had ruled out stale code for Jurassic World Evolution. The failure is
  deterministic in the anti-tamper's code and independent of these modes. That fits a tamper
  response, a check that fails and then corrupts a pointer on purpose, better than an emulation
  slip. Which check fails is open.
- **The sync was not changed.** The probe measures 12 to 15 µs a one-page protect on the phone
  (0.6 to 0.8 µs under desktop Wine), and the scan is a few dozen compares of that. A protect inside a
  pool-copied image does walk the image's whole relocation table (`iOS JIT IAT sync`), which is
  not proportional to the page. But no image of these titles has a pool copy, and no measurement
  here shows that walk costing anything, so it was left alone and is noted as a follow-up.

## The probe

`tools/compat/probes/protect.c`, built for x86-64 as `protect.exe`, put in
`Documents/Import/protect` with AFC and imported through the UI
(`pp ui --action import:Import/protect`, title `dir-protect`). Phases: `usd` reads the interrupt
time through a base register 100,000 times (the game's form, so FEX emits LDAPUR); `data`, `rx`
and `image` make 8,192 one-page `VirtualProtect` calls on an allocation and on 1 MiB of the
probe's own image; `exec` writes `mov eax, N; ret` into each page of a `PAGE_EXECUTE_READWRITE`
allocation, makes it `PAGE_EXECUTE_READ`, calls it and makes it writable again, 4,096 times.
`exec-rw`, the same in a `PAGE_READWRITE` allocation, runs only when named (below).

| Where | IPA | usd read | data | rx | image | exec round |
| --- | --- | --- | --- | --- | --- | --- |
| desktop Wine 11.18 | — | 0.4 ns | 0.7 µs | 0.8 µs | 0.7 µs | 1.4 µs |
| phone, `probe1` | `7deac3fa…` (0122) | 13.9 µs | 18.0 µs | 18.6 µs | 15.9 µs | (`exec-rw`: crashed, below) |
| phone, `probe2` | `7deac3fa…` | 14.6 µs | 14.3 µs | 14.7 µs | 13.1 µs | 86.3 µs |
| phone, `final/probe` | `57d12a62…` | 15.2 µs | 13.1 µs | 13.5 µs | 12.1 µs | 81.8 µs |

All checks were `ok` in each phone run listed with timings: the interrupt time never went back and
moved across a 50 ms `Sleep`, every protect succeeded, and every call returned what was written.
Each user shared data read on the phone is one Mach exception round trip (about 15 µs). A KSYSTEM_TIME read is three of them. That is
correct but slow, and a game that polls the interrupt time pays it on every poll.

In `probe1` the first `exec` phase used a `PAGE_READWRITE` allocation made `PAGE_EXECUTE_READ`
before the call. FEX's executable tracker had no entry for the page (`[iOS-xquery] MISS`, `NoExec
instruction in entry block: 702FDC0000`), and the call ended `c0000005` at a read of address 0.
Under desktop Wine the same phase passes. That is the `NoExec` family of the GoldSrc records
(2026-10-11, 2026-10-12), now reproduced by a probe. The probe keeps it as `exec-rw`, run only
when named, and it is a follow-up here.

## Phone runs

| Run | IPA | Title | Result |
| --- | --- | --- | --- |
| `fm22-base` | `3e81f622…` (before) | Football Manager 2022, Vulkan 16:9 | the LDAPUR fault loop from +1 s; no window in 6 min |
| `probe1`, `probe2` | `7deac3fa…` (af4be23, 0122) | `dir-protect` | timings above; `probe2` `done ok` |
| `fm22-long` | `7deac3fa…` | Football Manager 2022 | past the loop: 37,259 protect calls, then `c0000005` after 5.2 s, `AV WRITE of 712FED9FEF` at RIP `0x159782e84`. The bounded 30-minute replay therefore ended at 5 s; the five screenshots over the next 25 minutes show the library |
| `fm22-ua` | `2dfe58fc…` (0122 + 0123) | Football Manager 2022 | `c0000005` after 4.8 s, same RIP; 7 `[ua-ctx]` resumes, each changing only Rt |
| `fm22-notso` | `2dfe58fc…` | Football Manager 2022, TSO off | `c0000005` after 4.5 s, same RIP (`AV WRITE of 70C4608529`) |
| `final/hk` | `57d12a62…` (2b1501e) | Hollow Knight (`app-367520`) | first frame +9.83 s, `ok` |
| `final/p2` | `57d12a62…` | Portal 2 (`app-620`) | first frame +5.61 s, `ok` |
| `final/probe` | `57d12a62…` | `dir-protect` | `done ok` |
| `final/fm22` | `57d12a62…` | Football Manager 2022 | `c0000005` after 4.8 s, same RIP (`AV WRITE of 712C76E8EF`) |
| `final/install-jwe`, `final/jwe` | `57d12a62…` | Jurassic World Evolution | reinstalled from Epic (8.04 GB, about 2 min); 77,664 protects, `c0000005` after 7.7 s at `0x1488a3cf6` (`AV WRITE of 709BBD6937`); 32 `[ua-ctx]` resumes, each changing at most Rt |

Portal 2's `pp perf` was not run: 0122 changes the Mach fault service for an encoding that
looped before, not FEX's translation.

## Desktop

The workstation has no Epic launch token for these titles. Football Manager 2022's `fm.exe` is not
on it. Jurassic World Evolution's `JWE.exe` and DLLs are (from the 2026-10-07 store-auth work).
Run bare under desktop Wine 11.18 (no Epic arguments, a scratch prefix, no display), its main
thread made 554 protects, none to `PAGE_EXECUTE_READ`, and went on to create a window. It did not
take the page-by-page path, presumably because the licence step that precedes it had nothing to
check. So the number of protects before the menu under desktop Proton was not measured.

## What stays open

- **The anti-tamper's wild store (PLA-122, both titles).** It is deterministic at one RIP per title
  and independent of TSO and SMC checks, and the SIGBUS round trip is clean. Next: find which check
  fails. Candidates are what such a layer reads from the platform: debug state
  (`NtQueryInformationProcess`, thread contexts' debug registers), `CPUID`/`RDTSC`, the user shared
  data, and the bytes of its own image or of system DLLs' x64 export stubs. A trace of those calls
  and reads, compared with an authorised desktop Proton run, would name it.
- **A one-page protect costs 12 to 15 µs on the phone**, and a user shared data read 15 µs. Neither is
  a blocker here. The sync's relocation-table walk per page inside a pool-copied image is not
  proportional to the page. It is a follow-up only if a title shows it.
- **`exec-rw`:** code written into a `PAGE_READWRITE` allocation and made `PAGE_EXECUTE_READ` is
  not executable to FEX (`NoExec`). A follow-up.

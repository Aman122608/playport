# Evidence: JWE's `c000001d` was a missed self-modifying write in a half-trapped host page (PLA-241)

**Date:** 2026-10-12. **Kind:** a read of the game's file and FEX's invalidation tracker, phone runs
(dev app). **Linear:** [PLA-241](https://linear.app/playportdev/issue/PLA-241), for
[PLA-41](https://linear.app/playportdev/issue/PLA-41) (Jurassic World Evolution, Epic
`epic-373a4372c20540aca7fdd880d27fa49a`); the A/B touches
[PLA-243](https://linear.app/playportdev/issue/PLA-243). The owner was away; the conservative option
was taken at each choice. Run directories are under `$PLAYPORT_BUILD/ui-runs/`. Every JWE run was on
Vulkan for the session only (decision 0100).

**IPAs (dev):** before, `fc335c10b85d6e45e7739e66cd126bab6a86fe6cdde955b1e329b1d986560945` (83cac4d);
after, `adf030793b0960cfbcfd8a61f6f4a9c3daf7829e982ee169f66c39e60be56688` (fex 0033; installed at the
end).

## What the code at the fault is

Every run since madeira-unix 0132 ends `c000001d` on `Main` at guest RIP `0x142d74711`, "Invalid
instruction in entry block". `JWE.exe`'s `.ecode` (characteristics `0xe0000020`: code, read, write,
execute; mapped `PAGE_EXECUTE_WRITECOPY`, 0x80) holds there:

    142d74700: 81 35 00 00 00 00 3f 42 95 3f   xor dword [rip+0], 0x3f95423f   ; the next 4 bytes
    142d7470a: 30 e0 05 af                     ; encrypted: xor ah, al / add eax, ...
    142d7470e: 81 05 f2 ff ff ff 21 3e 75 1e   add dword [rip-0xe], 0x1e753e21 ; re-encrypt 0x142d7470a
    142d74718: c3                              ret

The `xor` decrypts the next four bytes to `0f a2 90 90` (`cpuid; nop; nop`); the `add` after them
encrypts them again. Read from the file, without that write, the bytes decode as `xor ah, al`,
`add eax, 0xf20581af`, and then `ff ff ff` at `0x142d74711`: two bytes into the `add`'s
instruction. So the guest did run the encrypted bytes. Its registers say so: every run
(`s11f-jwe-fix`, `s11f-jwe-fix2`, `s11g-jwe`, `s11h-jwe-strict`) ends with `rax=0xf20581b0`, which is
`add eax, 0xf20581af` on `eax=1`; `cpuid` would have overwritten `eax`. The bytes at `0x142d74711` were
the file's and were never meant to run. No caller or indirect branch is wrong: the block ran
through.

## Why FEX ran the old bytes

FEX's Windows invalidation tracker (`Source/Windows/Common/InvalidationTracker.cpp`) traps a page it
has compiled code from: it makes it `PAGE_EXECUTE_READ` (0x20). A write to it then faults, and
`HandleRWXAccessViolation` invalidates its code; a write into the running block ("inline
self-modifying code") is single-stepped and the rest recompiled. Those trap protects are JWE's
"one-page protects": 77,884 `0x80 → 0x20` and 75,416 `0x20 → 0x20` in `s11g-jwe`, all made by FEX
after a compile, not by the protection.

On iOS a host page is 16 KiB and holds four 4 KiB guest pages, and Wine maps a host page with the
union of their protections (`get_host_page_vprot`). Just before the fault the log shows:

    E 4C Invalid instruction in entry block: 142D74711
    NtProtectVirtualMemory(orig=0x142d74000 ... sz=0x1000 prot=0x20)
    mprotect_exec(0x142d74000, 0x4000, rw-)
    iOS NtProtect-sync-check: base=0x142d74000 sz=0x1000 old=0x80 new=0x20

Of the guest pages in host page `0x142d74000`, `0x142d75000` and `0x142d76000` had never run code
and were still 0x80, so the host page stayed `rw-`. The `xor` wrote without a fault, nothing was
invalidated, and the block went on with the bytes it had decoded. Where all four guest pages had been
trapped (`r--`), the same pattern works: `s11g-jwe` logs `Handled inline self-modifying code` once
and ten handled writes, all in fully trapped host pages.

A dev-only page-dump diagnostic (step 1 of the plan) was not built: the file's bytes, the
instruction stream and the identical `rax` answer the question, and the fix run below confirms it.

## The fix (fex 0033)

`patches/fex/0033`: on iOS the tracker's unit is the host page (`InvalidationTracker::TrapGranuleSize`,
16 KiB; `FEX_PAGE_SIZE` elsewhere, so other hosts are unchanged).

- `ProtectRWXIntervalsInternal` traps (and, for an untracked write, untraps) every RWX page in the
  host pages a range touches, so a trapped page never shares a host page with a writable RWX one.
- `HandleRWXAccessViolation` invalidates the whole host page: once the faulting guest page is
  writable again, so is its host page, and writes to the other three no longer fault.
- The inline self-modifying-code check (ARM64EC and WOW64) looks at the host page.

A host page that also holds plain writable data (not RWX) still cannot be trapped; that is the
same as before.

## Runs

| Run | IPA | Session setting | Outcome |
| --- | --- | --- | --- |
| `s11h-jwe-strict` | `fc335c10` | `env.FEX_STRICTINPROCESSSPLITLOCKS=1` | `exit=0xc000001d` after 31 s, same RIP, same `rax` |
| `s11h-jwe-fix` | `adf03079` | none, `--until first-frame+30` | **first frame at +38.26 s**, alive to the stop; two presents; screenshot black |
| `s11h-jwe-long` | `adf03079` | `env.WINEDEBUG=warn+seh`, `--until first-frame+120` | first frame +37.45 s; `exit=0xc0000005` after 45 s (below) |
| `s11h-jwe-long2` | `adf03079` | none, `--until first-frame+120` | first frame +37.17 s; the WatchDog's deliberate crash after 74 s (below) |
| `s11h-hk` | `adf03079` | none | Hollow Knight, first frame +9.58 s, its main menu |
| `s11h-p2` | `adf03079` | none | Portal 2, first frame +6.17 s |

In `s11h-jwe-fix` the site is handled as it should be:

    Handled inline self-modifying code: pc: 11A1E3358 rip: 142D74700 fault: 142D7470A
    Handled inline self-modifying code: pc: 11A1E363C rip: 142D7470E fault: 142D7470A

and four more sites of the same shape follow on other threads. The "Invalid instruction in entry
block" line still appears once per site: FEX decodes the next block before the write, and that
block is never run. Writes FEX handles rose from 10 to 50 in about the same span; in the old build
the other 40 were missed in the same way. Hollow Knight's invalidation census is unchanged
(`av 7`, against 8 in `s11g-hk`); Portal 2 has none.

## The A/B: `StrictInProcessSplitLocks`

FEX's `StrictInProcessSplitLocks` reaches the in-process handler through
`CTX->Config.StrictInProcessSplitLocks` (`HandleUnalignedAccess`), so the title's session setting
`runtime: env.FEX_STRICTINPROCESSSPLITLOCKS=1` sets it; no code was needed. `s11h-jwe-strict` ends
the same way at the same RIP with the same registers (31 s, against 32 s). The torn `lock xadd` of
PLA-243 does not decide this exit.

## After the first frame

JWE now presents (two frames, black) and goes on running its protected code. Two later ends:

- **`s11h-jwe-long2` (no logging):** in the last thread samples `Main`, `Render` and the WatchDog
  wait while `fTask W0` runs protected code at 91–96 % CPU. By +74 s the process has completed
  about 1.31 million guest split locks (`Handled unaligned atomic #1310720`; most sampled lines are
  `fTask W0`'s), about 40 µs each on the SIGBUS path (PLA-242): about 50 s of thread time. At 74 s the WatchDog runs its deliberate `mov dword [1], 0` at `0x140a6c820`.
  The likely cause is the split locks' cost: the protected job does not finish within the
  WatchDog's time.
- **`s11h-jwe-long` (`warn+seh`):** at 45 s, `fTask W3` creates a thread, suspends it, reads its
  context twice (`NtGetContextThread` returns `rip=0 rsp=0`, then a host PC as `rip`, not a guest
  one), terminates it, writes into `.ecode` page `0x142969000` from native code (handled as a
  self-modifying write), and then calls `0x142969620`, which is zeros in the file and in memory:
  `c0000005`, a read of address 1. This looks like the protection checking a thread's context and
  taking its failure path. It is not seen in the run without logging, which ends sooner on the
  WatchDog, so it is not shown to come every time.

## Next

1. The split-lock cost decides whether the protected job finishes before the WatchDog: the faster
   atomic path of PLA-242's follow-up (about 40 µs → 14 µs) is the next lever.
2. What `NtGetContextThread` returns for an x86-64 thread that is suspended before it starts, or
   while it runs native code: Windows returns the guest context (`RtlUserThreadStart` and the start
   address), not zeros or a host PC.

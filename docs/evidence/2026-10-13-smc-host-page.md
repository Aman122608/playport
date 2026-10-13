# 2026-10-13: code rewritten without a flush beside writable pages; Duck Paradox without its SMC default

**Linear:** [PLA-206](https://linear.app/playportdev/issue/PLA-206) (WoW64 SMC trap and 16 KiB host
pages), [PLA-115](https://linear.app/playportdev/issue/PLA-115) (Duck Paradox with SMC tracking off),
sweep 14 of [PLA-112](https://linear.app/playportdev/issue/PLA-112). The owner was away; the
conservative option was taken at each choice and is recorded below.

**Changes:**

- `tools/compat/probes/childabort.c`: a `noflush` phase (only when named). Four workers rewrite
  `mov eax, imm32; ret` and call it **without** `FlushInstructionCache`, 3 s in each of two layouts:
  `4k`, the code in its own 4 KiB RWX allocation; `rw-neighbour`, the code in page 0 of a 16 KiB
  allocation whose pages 1–3 are read-write data the worker writes on every round (one iOS host page).
  x86 needs no flush after a store to code, so every call must return the value just written.
- `patches/fex` 0038: in a WoW64 process, code in a host page the write trap cannot cover is
  compiled with per-instruction code checks.
- `app/PlayportKit/Titles/compat-defaults.json`: Duck Paradox's default
  `runtime: env.MADEIRA_KEEP_SMC=1` is removed. The game now reaches its title menu with SMC
  tracking off.

**IPAs (dev):** before, `f39cd151b99d00a0f3ac2f68dcb12d15794d43245881bbffe44099f838d9f2f0` (the
branch's previous build); a first attempt at 0038, `712e8d7615ea308f41b912a7fba739dc66a6d8001354756fed9ad5664189cce0`
(not kept, below); final, `408be0ea453111636b3ca874fd115d4d6cb9e5a584ec5e401dddf80178dc297d`.
Run directories are under `$PLAYPORT_BUILD/ui-runs/s14-*`.

## PLA-206: what fex 0033 already covers

fex 0033 (2026-10-12) traps every RWX page of a 16 KiB host page and invalidates the whole host
page on a write fault, in both the ARM64EC and the WOW64 module. The probe on `f39cd151`
(`s14-dir-childabort-14-before`, `s14-dir-childabort-14-i386-before`):

| Layout | x86-64 | i386 (WoW64) |
| --- | --- | --- |
| `noflush 4k` | ok, 0 of 32,703 stale | ok, 0 of 56,160 stale |
| `noflush rw-neighbour` | **FAIL, 914,382 of 928,900 stale** | **FAIL, 4,139,100 of 4,204,804 stale** |

So the case the 2026-10-12 record left open (a 4 KiB RWX allocation rewritten without a flush; 7 of
8 stale then) is fixed by 0033. A code page that shares its host page with a **writable data page**
is not: 63 of every 64 calls return the old value (only the first call on each new page is right).

- **i386 (WoW64):** the window's host page gets the union of its four Wine pages' protections
  (`wow64_protect.h`). The trap makes page 0 read-only; pages 1–3 stay read-write, so the host page
  stays writable and a store to the code never faults.
- **x86-64:** the trap does make the host page read-only (`[wr-strip] … host_prot=1`). The first
  store to a data page then faults, and Wine's write-strip heal (`signal_arm64_ios.c`, ml552)
  restores read-write on the whole 16 KiB host page without telling FEX, so the code page is
  writable again and later stores to it do not fault either.

## PLA-206: the change (fex 0038)

Such a host page cannot be trapped, so its code checks itself:

- For the WOW64 module, `InvalidationTracker::QueryExecutableRange` splits an RWX range at a host
  page it covers in part when that host page holds a committed writable page outside the RWX ranges
  (`NtQueryVirtualMemory` on its other pages). That part is reported as `FullSMC`.
- The frontend compiles blocks in a `FullSMC` range with `ForceFullSMCDetection`, the per-instruction
  code check FEX already uses for Mono's tailcall sites. The disk cache is not used for them.
- A page made writable beside RWX code invalidates its host page's code, so that code is compiled
  again with the checks.
- A range aligned to host pages costs nothing (Mono's code chunks, typical JIT regions). A partial
  one costs up to three `NtQueryVirtualMemory` calls per range query. With SMC detection off
  (Mono's backpatcher, fex 0003) nothing changes. Off iOS nothing changes.
- The log line `[smc-hostpage] #N base-end` (the first 8 and every 4096th) names each such range.

**Choice:** the code checks, not a fault service for the writable neighbours. Trapping the
neighbours too would make a game's data pages read-only (`VirtualQuery` would show it, a
`VirtualProtect` round trip would restore read-only, and a syscall writing into such a page would
fail), and every data store beside code would cost a fault and a retranslation.

**x86-64 is not changed.** The first build (`712e8d76`) enabled the checks in both modules. The i386
probe passed, but on x86-64 all four `rw-neighbour` workers stopped at once
(`s14-dir-childabort-14-after`): the first thread whose check failed waited for FEX's code
invalidation mutex for ever (`[fexlock] STUCK write-wait … owner tid=0040`), and the other three
waited behind it. The final build enables the checks only in the WOW64 module. An x86-64 process
still runs stale code in this layout, as before; the fix belongs with Wine's write-strip heal (tell
FEX before making a trapped host page writable) or with the deadlock. That is a follow-up.

The probe on `408be0ea` (`s14-dir-childabort-14-final`, `s14-dir-childabort-14-i386-final`):

| Layout | x86-64 | i386 (WoW64) |
| --- | --- | --- |
| `noflush 4k` | ok, 0 of 32,147 stale | ok, 0 of 55,607 stale |
| `noflush rw-neighbour` | FAIL, 968,058 of 983,428 stale (unchanged) | **ok, 0 of 78,170 stale** |

The i386 workers make about a fifth as many calls in `rw-neighbour` as before: each rewrite is now
a failed check, an invalidation and a new translation, where before the old translation ran.
`childabort: done ok` for i386. Desktop Wine 11.18 passes both layouts on both architectures (0 of
40–56 million calls stale).

## PLA-115: Duck Paradox with SMC tracking off

Duck Paradox (GOG `1816031813`) was installed through the UI (`s14-duck-install`, 1.0 GB).

| Run | IPA | Settings | Result |
| --- | --- | --- | --- |
| `s14-duck-nosmc-before` | `f39cd151…` | Vulkan, 16:9, `env.MADEIRA_KEEP_SMC=0` (overrides the default) | `[smc-off] Mono backpatcher detected: SMC detection disabled`; first frame +5.82 s, `until first-frame+30` ok; title menu (Play, Options, Quit) |
| `s14-duck-nodefault` | `712e8d76…` | Vulkan, 16:9, default removed | the same; first frame +5.53 s, title menu |
| `s14-duck-final` | `408be0ea…` | Vulkan, 16:9, default removed | the same; first frame +5.43 s, title menu |

On 2026-10-11 and on `b1f0278b` (2026-10-12, after wine-pe 0038/0039) the same setting ended the
game 1 s after start (exit `0xf8f08f70`, a guest `call r11` with `r11 = 0`). One of the changes
since then fixed it: fex 0033–0037, wine-pe 0040–0042, wine-unix 0036 or madeira-unix 0133–0137.
Which one was not bisected; fex 0033 (code invalidated by host page) and fex 0036 (split atomics
completed without their alignment fault) are the likeliest. The default is removed, so the game now
runs with tracking off, as upstream does for Mono. The game was uninstalled afterwards
(`s14-duck-uninstall`); the phone had 5.8 GB free.

## Gates (`408be0ea`)

| Run | Title | Result |
| --- | --- | --- |
| `s14-hk` | Hollow Knight (`app-367520`) | first frame +9.80 s, `until first-frame+10` ok; title menu |
| `s14-p2` | Portal 2 (`app-620`) | first frame +5.91 s, `until first-frame+10` ok; the Source splash |

Neither logged a `[smc-hostpage]` line.

## Host

`./pp test --quick` passed; `./pp test --swift PlayportKit --filter CompatDefaultsTests` passed
(the key-merge test now builds its own Duck Paradox entry).

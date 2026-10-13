# Evidence: anonymous JIT aliases end at MEM_RELEASE; an i386 FlushInstructionCache reaches FEX (PLA-205, PLA-201)

**Date:** 2026-10-12. **Kind:** phone runs (dev app) with probe imports, log analysis, host tests.
**Linear:** [PLA-205](https://linear.app/playportdev/issue/PLA-205) (stale anon JIT alias),
[PLA-201](https://linear.app/playportdev/issue/PLA-201) (code rewritten in place not translated
again); titles [PLA-115](https://linear.app/playportdev/issue/PLA-115) (Duck Paradox),
[PLA-114](https://linear.app/playportdev/issue/PLA-114) (Football Manager 2022).
**Changes:** `patches/madeira-unix` 0124 and `patches/wine-pe` 0039 (commit e4d8aa6).
The owner was away, so the conservative option was taken at each choice below. **IPA:**
`Playport-26.5-b1f0278b.ipa` (dev), sha256
`b1f0278bac6553646ec639b55679d5dfc7d40c5246eefa1c3458914a57a17660`, built from e4d8aa6 and installed
in place. The "before" runs are on `ad449e16…` (9d40a49,
[2026-10-12 noexec](2026-10-12-noexec-rw-rx.md)). Run directories are under
`$PLAYPORT_BUILD/agent-notes/speedrun/jitalias/`.

## Summary

- **PLA-205, root cause:** the anonymous-alias table (guest JIT memory that `mprotect_exec`
  remaps from the JIT pool, with a `user VA -> pool RW` entry for the store emulator and the
  Mono bridge) was cleared only at its process's exit or when its pool range was handed out
  again. After `MEM_RELEASE` the entry went on describing the address. A later
  `PAGE_READWRITE` allocation there was forced read-only (ml638 in `set_protection`), its
  stores were emulated into the dead pool slot, and FEX translated zeros.
- **PLA-205, fix (madeira-unix 0124):** `free_pages` (both the whole-view and the partial path)
  and `free_pages_preserve_placeholder` retire, before the host pages go, every entry inside
  the host-page range they release. This is `ios_jit_anon_alias_release`. Three choices:
  - An entry the range covers in part keeps the pages outside it. They become new entries,
    written before the old one is retired, so a lock-free reader (the fault handler) always
    finds an entry mapping an address to the same pool byte. The Mono bridge's entry follows
    (published for each remainder, retired for the old base unless a remainder keeps it).
  - With no free slot for a remainder, the entry is kept whole, as before, and logged.
  - **The pool range is not returned** (as before). Only the alias ends; nothing else about pool
    accounting changes.
- **`MEM_DECOMMIT` keeps its alias.** Decommit keeps the mapping, and `decommit_pages` zeroes
  through the alias (FEX's `VirtualDontNeed` decommits as a cheap bzero and touches the pages
  again; task #22 in its comment). A recommit then writes through the alias as before.
- **FEX's code buffers and the Mono bridge.** On the ARM64EC side FEX's code buffers are tail
  carves (`NtAllocateVirtualMemoryEx` EC_CODE). They have no anon-alias entry and are freed by
  their own path (ml438), before `free_pages`. An anon-alias entry belongs to guest JIT memory
  (Mono's, V8's), freed whole when its owner is done with it, and its user VA is unmapped by that
  same free. The bridge's lookup already treats an entry whose generation is even as dead, and the
  table's existing purges (process exit, handout) retire it the same way.
- **PLA-201, re-measured:** **not** fixed by wine-pe 0038. On `ad449e16…` the childabort probe's
  rewritten code still returned the old value on 87.5 % of calls. That is exactly 7 of every 8: the
  first call on each new page is right, every later rewrite of it is stale.
- **PLA-201, root cause:** two gaps in the i386 (WoW64) process, together:
  1. **Wine's i386 kernelbase `FlushInstructionCache` is a no-op** ("x86 caches are coherent").
     The probe's flush never made a syscall, so wow64's `NtFlushInstructionCache`, which calls
     FEX's `BTCpuFlushInstructionCache2`, never ran. The ARM64EC kernelbase does call it, so x86-64
     processes were not affected.
     **Amended 2026-10-13 ([arm64ec-flush](2026-10-13-arm64ec-flush.md), PLA-325):** wrong. clang
     defines `__x86_64__` for ARM64EC too, so the ARM64EC kernelbase had the same no-op;
     wine-pe 0044 fixes it.
  2. **FEX's write trap does not cover the page.** FEX traps writes to translated RWX code by
     protecting the 4 KiB page `PAGE_EXECUTE_READ`. In the WoW64 window the host page is
     16 KiB, and its native protection is the union of its four Wine pages (wow64_protect.h:
     "enforced only logically"). The probe's 4 KiB `VirtualAlloc` commits the whole 16 KiB host
     page RWX, so the host page stays writable. The trap never fires: FEX's `[inv-census]` shows
     `av 0` in every run.
- **PLA-201, fix (wine-pe 0039):** the i386 `FlushInstructionCache` calls
  `NtFlushInstructionCache`, as Windows' does. The write trap's 16 KiB gap is not changed. A
  title that rewrites code in such a page **without** flushing is still not translated again.
  That is a follow-up (below), not a safe change for this sweep: trapping the whole host page
  would fault on its writable neighbours, and nothing services those faults.
- **`MADEIRA_KEEP_SMC=1` as a control:** no change (87.5 %). That switch only acts once FEX
  detects Mono's backpatcher (`[smc-off]`), and the probe has none.

## The probes

**`protect.exe exec-rw-reuse`** (title `dir-protect 2`). It runs `exec-rw` first, then `data`, `rx`,
`image` and `exec`, then `exec-rw` again on the freed range:

| IPA | Run | exec-rw | data | rx | image | exec | exec-rw-reuse |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ad449e16…` | `noexec/r3/probe` | 74.2 µs, ok | 12.2 µs | 18.5 µs | 18.5 µs | 74.8 µs | zeros, `c0000005` |
| `b1f0278b…` | `r1/probe` | 81.3 µs, ok | 15.2 µs | 20.1 µs | 19.5 µs | 83.1 µs | **71.9 µs, ok (0 stale)** |

`protect: done ok`. The run logged the retirement of the 32 entries the `exec` phase left in its
1 MiB range: `[jit-alias] PLA-205 release #1 0x7033c00000+0x100000: entry 2
0x7033c00000..0x7033c04000 … retired, 0 remainder(s) kept` up to #32. No `[alias-prot] ml638` or
`[store-noalias]` line follows. The per-call times move by a few µs between single runs, both ways.

**`childabort-i386.exe crash`** (title `dir-childabort`). Four workers rewrite `mov eax, imm32; ret`,
flush and call it every 8th iteration, with a new page every 64th:

| IPA | Run | Calls | Old value returned | FEX `[inv-census]` (parent, last line) |
| --- | --- | --- | --- | --- |
| `ad449e16…` | `m1/ca` | 73,525 | 64,334 (87.5 %) | n=36,864: protect 9,078, aligned 27,784, av 0 |
| `ad449e16…`, `MADEIRA_KEEP_SMC=1` | `m1/ca-keepsmc` | 154,084 | 134,821 (87.5 %) | (no `[smc-off]` line) |
| `b1f0278b…` | `r1/ca` | 44,818 | **0** | n=67,584: protect 5,570, aligned 62,012, av 0 |

Before the fix the "aligned" invalidations were about the frees alone. Each worker frees a code
page and a 1 MiB block every 64 iterations: 4 × 2 × 145,000 / 64 ≈ 18,000, plus heap frees. After
the fix they include the flushes. The workers did fewer iterations in the phase: each flush is
now a syscall and an invalidation, and the phase's length is set by its three crash children, not
by a fixed count. All three crash children exited `c0000005` as designed, and `childabort: done ok`.

## Phone runs

All on `b1f0278b…`, on the charger:

| Run | Title | Result |
| --- | --- | --- |
| `r1/install` | — | installed in place (26 s), container kept |
| `r1/probe` | `dir-protect 2`, `exec-rw-reuse` | every phase `ok` (above) |
| `r1/ca` | `dir-childabort`, `crash` | 0 stale (above) |
| `r2/hk` | Hollow Knight (`app-367520`) | first frame +10.15 s, `ok`; title menu at +10 s |
| `r2/p2` | Portal 2 (`app-620`) | first frame +6.17 s, `ok`; the Source splash |
| `r2/p2perf` | Portal 2 `pp perf`, The Cold Boot | below |
| `r3/install` | Duck Paradox (GOG `1816031813`) | installed through the UI |
| `r3/duck-default` | Duck Paradox, compatibility default | `[smc-off] … KEPT`, first frame +6.07 s, `ok`; its title menu |
| `r3/duck-nosmc` | Duck Paradox, `env.MADEIRA_KEEP_SMC=0` | unchanged: `[smc-off] … disabled`, exit `0xf8f08f70` after 1 s |
| `r3/uninstall` | Duck Paradox | uninstalled through the UI |
| `r4/fm22` | Football Manager 2022 (Epic `29c30b4114b247678453c84ae156715d`), Vulkan 16:9 | unchanged: first window +6.48 s, no first frame within `--wait 180` (exit 2), no `c0000005` |

Portal 2 is an i386 process, so wine-pe 0039 reaches it. Its FEX invalidations up to first
frame+10 are the same as on `ad449e16…` (`aligned 35,814` at n=35,840 in both), so it does not flush
in that window. Neither Hollow Knight, Portal 2, Duck Paradox nor Football Manager 2022 logged a
PLA-205 retirement. None of these titles frees anonymous-alias memory in the time measured.

Football Manager 2022 was asked for a longer wait as two chained 180 s steps. That is not
possible: a second `pp ui` relaunches the app. One 180 s run was made instead.

Portal 2, `pp perf --title app-620 --secs 120 --pad first-frame+30:p2-cold-boot --pad
first-frame+70:p2-walk`, `pp perf --compare … --window 75:120`:

| Run | IPA | FPS | GPU ms | M instructions a frame | CPU % | CPU mW |
| --- | --- | --- | --- | --- | --- | --- |
| `noexec/r4/p2perf` | `ad449e16…` | 60.0 | 15.19 | 58.6 | 121 | 225 |
| `r2/p2perf` | `b1f0278b…` | 60.0 | 15.19 | 58.9 | 123 | 229 |

Unchanged.

## Host

`./pp test --quick` passed with both patches applied (series, trailers, names, secrets,
`tools/tests`, the app's C tests).

## What stays open

- **FEX's write trap in the WoW64 window works per 16 KiB host page, not per 4 KiB Wine page.**
  An i386 title that rewrites translated code in place without `FlushInstructionCache`, in a page
  that shares a host page with writable pages, still runs the old translation. Fixing it needs
  either a fault service for the writable neighbours of a trapped page or 16 KiB tracking in FEX's
  WoW64 module. A follow-up.
- **A freed alias's pool range is still not reused** while its process lives (as before 0124). A
  title that churns guest JIT regions grows the pool head. The table's slots are now reused.
- `NtUnmapViewOfSection` of a section made executable through the anonymous path is not covered.
  No title has been seen to do it.

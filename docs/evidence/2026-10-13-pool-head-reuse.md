# 2026-10-13: A dead process's image ranges go to later image copies (PLA-213)

**Linear:** [PLA-213](https://linear.app/playportdev/issue/PLA-213) (sweep 14b of
[PLA-112](https://linear.app/playportdev/issue/PLA-112)).
**Change:** `patches/madeira-unix` 0138, and the probe `tools/compat/probes/poolstress.c`.
**Questions:**
- Why does every process a title starts keep its share of the JIT pool's head after it ends?
- Can that head be given back without running code on a page that lost execute permission?

**Result:**
- Madeira already put a dead process's head ranges on a free list. No image range was ever
  handed out again, because every image copy poisons its own `.data` pages.
- A freed image range is now taken by a later image copy whose writable pages cover every poisoned
  page in it. Twelve children started one after another took the head from 20 to 98 MiB. Before
  the change they took it to 284 MiB. After ten rounds the head was 98 MiB, where it had been 424.
- Every child's checks passed, and no copy had a non-executable `.text` page.
- The head cursor does not move down. A freed range still has its poisoned pages, so it can never
  hold FEX's 128 MiB code buffer, and it is never counted as room.

## Why the head was not reused

An image copy makes each writable section read-write on the pool's executable alias (`mprotect`
RW in `mprotect_exec` and in `ios_jit_copy_module_for_child`). On the debugger-blessed pool this
takes EXEC out of those pages' `max_prot` for good (Madeira's ml140 note): no later `mprotect` can
give it back. So when a process ends, every image range it freed has non-executable pages where
its `.data` was. Madeira's reuse guard (ml242) refused any range with such a page, and dropped it
from the list.

The old run shows this. Each x86-64 child freed 22 ranges (33.4 MiB) and each i386 child freed 5
(7 MiB). There were 145 `[jit-pool] POISONED range … dropped` lines, and the 10 `reused freed
range` lines were all small guest JIT blocks.

## The change (madeira-unix 0138)

- **At free:** a range whose pages are not all executable stays on the free list, marked as
  poisoned (`[jit-pool] RECLAIM … (grace 3s, N poisoned)`).
- **The image's shape:** an image copy (the loader's and a child's ntdll copy) works out its
  writable data sections from its PE headers (`ios_pool_image_shape`). Each becomes a span of
  16 KiB pages relative to the start of its allocation, after the data-align shift. A writable
  code section is not counted, since the loader keeps its EXEC.
- **The search:** the copy looks first for a marked range of its own size (usually a dead copy
  of the same image), then for any larger one. It takes the range only when every page without
  EXEC lies on one of its writable pages (`ios_pool_range_fits_image`). That check:
  - reads `max_prot` one region at a time with `mach_vm_region` and changes nothing;
  - refuses a hole in the pool or a query that failed.
- **What does not change:**
  - the 3 s grace after the process ended;
  - the double-handout check;
  - clean ranges first for everything else. Anonymous JIT blocks and the code-buffer tail never
    take a marked range.
- **A full free list (256)** gives up a marked range rather than the new one.
- **The log:** `[jit-pool] PLA-213 reused poisoned range off=… size=…: its N non-exec page(s) are
  all the new copy's writable pages`. `[copy-verify]` still checks each copy's `.text`.

Such pages are the new copy's writable pages, which it makes read-write in turn and never runs. Every
page it might run (code, headers, read-only data, x18 trampolines) still has EXEC.

## The probe

`poolstress.exe [ROUNDS [PAUSE_MS]]` (default 6 rounds, 3500 ms) is built for x86-64 and i386
into one folder. It was put in `Documents/Import/poolstress` with AFC and added through the UI
(`dir-poolstress`).

Each round starts `poolstress.exe child`, an x86-64 process whose ARM64EC images are copied into
the pool, and then `poolstress-i386.exe child`, an i386 WoW64 process. Each child is started only
after the one before it exited and the pause passed. Each child:
- loads user32, shell32, shlwapi, ole32 and advapi32;
- calls `CharUpperA`, `wsprintfA`, `SHGetFolderPathA`, `PathFindFileNameA`, `CoCreateGuid` and
  `StringFromGUID2`, and the registry;
- calls the C runtime's `qsort`, `snprintf` and `strtod`;
- runs code it writes into an RWX page;
- checks every result, and exits 0 only when all passed.

A fault in a reused copy would end the child with `c0000005`. Desktop Wine 11.18 passes it.

## Phone

All runs are on the iPhone18,4 with iOS 27.0, plugged in. Run directories are under
`$PLAYPORT_BUILD/agent-notes/speedrun/s14b/runs/`.

| Run | IPA | Children | Head after each round (MiB) | Head, room at the end | Reused image ranges | Failed children |
| --- | --- | --- | --- | --- | --- | --- |
| `before` | `408be0ea…` | 6 + 6 | 76, 116, 156, 197, 237, 277 | 284, 180 | 0 (145 dropped) | 0 |
| `after` | `d99742fc…` (first build of 0138) | 6 + 6 | 76, 94, 98, 98, 98, 98 | 98, 366 | 126 | 0 |
| `v3-poolstress` | `91e30448…` (final) | 6 + 6 | 76, 94, 98, 98, 98, 98 | 98, 366 | 126 | 0 |
| `before10` | `408be0ea…`, `10 3000` | 10 + 9 | (rising) | 424, 40 | 0 | round 10's x86-64 child `c0000005` |
| `final10` | `8ced8eb1…` (second build), `10 3100` | 10 + 9 | 98 from round 3 | 98, 366 | 215 | round 10's x86-64 child `c0000005` |

- **The head cursor** (`bump=` in `[jit-pool] RECLAIM`), each child in turn:
  - after the change: 0x4b28000, 0x4f58000, 0x5d9c000, then 0x61cc000 for every later child;
  - before it: 0x4b28000, 0x521c000, 0x7374000, 0x7a68000, … 0xf34c000.
- **The reused ranges:** most had 1 to 5 non-executable pages. The largest (libarm64ecfex.dll,
  0x490000) had 23, and a few had 20.
- **The checks on the final IPA:**
  - every copy logged `[copy-verify] … OK` (no `BORN NON-EXEC`);
  - no `[pool-poison]`, `[pool-dup]` or `[exec-recover]` line;
  - every child's checks passed (`poolstress: parent: done ok`).
- **The round-10 crash** happens on the old IPA too: the same child, at the same moment. Before
  it, FEX logged `[jit-alias] ml1106 IosAliasEntries FULL (256 entries): image … NOT registered`,
  and the child then jumped to a wild address (`0xfffffffef7768be7`). Every image any process
  loads is pushed into every live emulator's 256-entry alias table, and nothing removes a dead
  child's entries. After about nine rounds the parent's table is full. This is a separate bug
  (a follow-up), not one of the change's.
- **Room:** at the end of six rounds the room is 366 MiB, against 180 before. At the end of ten
  rounds it is 366 MiB, against 40.

### No poisoned page reached code

| Probe | Run | Result |
| --- | --- | --- |
| `protect.exe exec-rw-reuse` (`dir-protect 2`) | `protect` (`d99742fc`), `v3-protect` (`91e30448`) | every phase ok, `protect: done ok` |
| `childabort.exe` x86-64 (`dir-childabort-14`, dxdiag and crash) | `dir-childabort-14` (`d99742fc`), `v3-childabort` (`91e30448`) | `childabort: done ok`, 0 stale |
| `childabort-i386.exe` (`dir-childabort-14-i386`, dxdiag and crash) | three on `d99742fc`, three on `408be0ea` (the old IPA) | see below |

**The i386 childabort is flaky on both IPAs.** Every one of its checks passed in all six runs.
- **Ending:** after the crash phase, the run ended `done ok` on the old IPA in 2 of 3 runs, and on
  the new one in 0 of 3. A failed run either hung or ended the app.
- **The cause, on both IPAs:** a worker thread was killed while it held `fd_cache_mutex`
  (`wine client error: write: Bad file descriptor`, then `[section] thread abort: … leaves the
  section of mutex … (fd_cache_mutex)`). The old IPA's hang (`…-base2`) has the same line.
- **A crash child that never started:** one of the three failed with `0xfffffffe` (its main image
  map returned `c0000008`) on both IPAs.
- **Not the change:** none of these runs reused a range (`PLA-213 reused` 0, `POISONED range` 0).
  The crash children end within the 3 s grace.

It is a thread-exit race in the killed-thread work, not a pool one, and it is a follow-up.

### A title with a prerequisite

Counter-Strike (`app-10`, `v3-cs`) ran its Steam VC++ 2019 x86 entry again. Its record was a failure
under another build (decision 0083). Since decision 0096 the entry runs the installer directly, not
the `.cmd` chain:
- the installer exited `0x666`;
- `room_for_game_mb=462 (needs 320)`, and the game started in the same session;
- first frame at +6.43 s, on its main menu (New game, Find servers, Options, Quit).

The game started 0.25 s after the installer ended, within the 3 s grace, so its own copies did not
reuse the installer's ranges: head 34 → 41 MiB, `head_free_mb` 14 throughout. A Steam `.cmd` pair
or a launcher whose processes outlive the grace reuses the head (the probe above). One started right
after a process ends does not.

### Gates (final IPA)

IPA `Playport-26.5-91e30448.ipa`, sha256
`91e30448fb87917f893d12139952b8a456d578ca70ba4b715c93d328928df91d`.

| Title | Run | First frame | Screen |
| --- | --- | --- | --- |
| Hollow Knight (`app-367520`, `--until first-frame+10 --shot`) | `v3-hk` | +10.00 s | main menu (Start Game, Options, Achievements, Extras, Quit Game); head 83, tail 161, room 269 |
| Portal 2 (`app-620`, `--until first-frame+10`) | `v3-p2` | +6.14 s | (no screenshot); head 27, tail 161, room 325 |

The heads are the same as in the 2026-10-12 record (83 and 27 MiB). A single game process frees
nothing, so 0138 does not act there.

## Host

- `./pp test --quick` passes (series and trailers, names, secrets, `tools/tests`, the app's C
  tests).
- The probe passes under desktop Wine 11.18 (`poolstress.exe 2 200`: four children, exit 0).

## Not done

- **The head cursor does not come back down.** Lowering it is safe only over clean ranges at its
  top. Image ranges are never clean, so it would almost never act, and FEX's 128 MiB code buffer
  needs a clean contiguous range. Recovering the room would need image copies that never poison:
  their `.data` would have to live outside the blessed pool.
- **A game started within 3 s of its prerequisites** does not reuse their ranges (above). The
  decision 0094 bound (320 MiB of room) is unchanged.
- **The JWE and FM2022 lockouts:** neither was played.

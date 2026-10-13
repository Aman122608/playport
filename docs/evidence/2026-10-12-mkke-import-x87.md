# Mortal Kombat Komplete Edition: imported by clone, past dxdiag, x87 at 64 bits breaks it

**Date:** 2026-10-12. **Title:** Mortal Kombat Komplete Edition (Steam 237110), imported as a
Local title from the owner's Windows install folder ([PLA-36](https://linear.app/playportdev/issue/PLA-36)).
**Phone:** iPhone18,4, iOS 27.0, dev build, Vulkan, 16:9.
**IPA:** `34055093c5f79e27e25a479b6ee8ff2b26c6eb99dc7dba7bf4fffec83e4ca168` (c3cda59).

## The import (PLA-199)

In sweep 2.5 ([dxdiag evidence](2026-10-12-dxdiag-child-abort.md)) the import stopped before
copying: *Not enough free space: 20.5 GB needed, 15.8 GB free*. The importer clones a file on
the same volume, which takes no space, but its check counted the whole folder, 5 % and the
reserve.

c3cda59 (`GameImporter`, PlayportKit) checks a folder that sits on the stage's volume (the
same `st_dev`, Apple platforms only) for the reserve alone. A file whose clone fails falls back
to a copy only after its own check (its size, 5 % and the reserve), so a volume that cannot
clone stops with *Not enough free space* before it writes; a write that still runs out ends
in the existing `ENOSPC` error. A zip, or a folder on another volume, keeps the full check.
Host tests: `GameImportTests` (a clone needs only the reserve; a failed clone is checked before
its copy, nothing copied when refused, the import completes when the space is there; the full
figure for a copy).

On the phone, the owner's folder (1,530 entries) was pushed into `Documents/Import` as before
(274 s), leaving 13.8 GB free. `import:Import/Mortal Kombat Komplete Edition` (run
`20261012T030123`) went from `started` to `done` in under a second, 19.27 GB in 1,508 files,
and free space stayed at 13.8 GB. The 2026-10-11 build would have refused it. The `Import`
copy was then deleted; free space stayed 13.8 GB, because the game's clone still holds the blocks.

## The play

| Run | Settings | Outcome |
| --- | --- | --- |
| `20261012T030216` | Vulkan, 16:9 | first window +3.7 s, first frame +5.3 s; `dxdiag.exe /whql:off /t …` ran as a child and exited 0; title art at +30 s (`ok`) |
| `20261012T030357` | as above, to +150 s | title screen with *PRESS SPACE* at 45 s; `c0000005` at 69 s |
| `20261012T030743` | as above, to +120 s | *Runtime error! R6025* (pure virtual function call) dialog from `MKKE.exe` |
| `20261012T031110` | ordering vector and memcpy/set TSO on | `c0000005` at 69 s, same instruction |
| `20261012T031433` | `"x87Reduced": false` (80-bit x87) | no exception in 150 s: title, then the attract mode loads Goro's Lair and plays a 3D fight (frame count 8,224) |

So the dxdiag hang is gone: the computer-name keys and the lock-cycle fix of 0c51652 hold for
the game itself.

The crash is at `MKKE.exe` `0x7145bd`, reached from `0x42c9de`, on three threads at once
(`0x002c`, `0x00cc`, `0x0034` in the TSO run). The function is a lock-free list pop. It loads
the 64-bit head, a pointer and a tag, with `fild qword [ecx]; fistp qword [ebp-0x20]`, the
32-bit MSVC idiom for an atomic 64-bit load. It then reads `[eax]`, the next pointer, before
its `lock cmpxchg8b`. At 64-bit x87 precision (`X87ReducedPrecision=1`, decision 0048), FEX
moves the value through a double. With the tag `0x84280002` above it, the pair's magnitude as an
integer is near 2^63, so a double keeps the pointer only to 1,024 bytes. The faulting addresses are round:
`0x32030000` and `0x2e2c0000`, each the first byte after a 1 MB block the game committed
(`0x31f30000` + `0x100000`, `0x2e1c0000` + `0x100000`), in a region reserved and not
committed. When the rounded pointer lands on readable memory, the game goes on with a wrong
node, as in the R6025 run. Decision 0048 names this cost: 64-bit integers through
`fild`/`fistp` beyond 2^53. With 80-bit x87 the same idle ran for 150 s into the attract-mode
fight. That is one run, not a play with input.

## After

The game was uninstalled through the UI (`20261012T032004`: folder, record and stage gone).
It only works with a setting on its page, and keeping it would leave about 14 GB free, under
the 20 GB this sweep keeps. Free space: 33.0 GB.

Gates on the same IPA: Hollow Knight (`app-367520`, `20261012T031825`) first frame +9.0 s,
title menu at +10 s; Portal 2 (`app-620`, `20261012T031921`) first frame +5.8 s, Source
splash. Both `ok`.

## What stays open

- **MKKE needs 80-bit x87 by default.** The game is a Local import, which has no Steam app ID
  for `FEXProfile.protonApps`, and no compatibility default keys a `dir-` id yet (decision
  0074). Choose how a Local title gets a profile entry, for example by executable name or by
  the `steam_appid.txt` hint, or whether FEX should keep a `fild`/`fistp` pair of 64-bit
  integers exact at reduced precision.
- **Play with input** (press Space, a fight) at 80-bit x87; frame rate is not measured.

# 2026-10-08: Proton parity for the staged Wine modules (PLA-78)

**Change:** commit `77e125e` on `dev/pla-78-proton-dll-parity`
([decision 0069](../decisions/0069-wine-modules-proton-parity.md)). Every production
Wine module per architecture is staged from `build/generated/wine-pe-*.tsv`.
**Phone:** iPhone18,4, iOS 27.0, dev app, Vulkan/DXMT backends as each game's page
has them saved; unattended for the later runs.

## IPAs

| IPA | sha256 | bytes | uncompressed | files |
| --- | --- | ---: | ---: | ---: |
| dev, before (`08836ab`) | `cdbeaf3322fe4bf0f79f55f7b2fb514edbd6bb7594739082467473e8a64a5edb` | 678,882,758 | 678,491,850 | 1,623 |
| dev, after (`77e125e`) | `4a738948581b55cf7aed68d31902de7b0bbe34abc2eb5f39ec588a9c440245c1` | 1,181,431,404 | 1,180,872,096 | 2,379 |
| release, after (`77e125e`) | `dd7ac082c834d9b0c691e0a45afa338176ac3e5bd0bd9d4dd5035e6feca3262a` | 1,175,026,597 | | |

The IPA is stored, not deflated, so the dev IPA grows by 502,548,646 bytes and the
bundle by 502,380,246 bytes (479.1 MiB) for 756 more files. Deflate-6 of the added
files is about 48 MiB. The release IPA from before (`c7fe5dbf…`, `0d1c4c7`, same
runtime) had been pruned before it was measured; it has the same set of runtime
files, so its growth is the same. Both IPAs pass `pp verify` (80 and 84 checks).

## Staged sets

| Set | Before | After | Rule |
| --- | ---: | ---: | --- |
| `arm64ec-windows` | 149 | 650 | all 647 built production modules (FEX `xtajit64`, `P2-ec` ntdll, DXMT `d3d11`/`dxgi`/`d3d10core` replace Wine's), DXMT `winemetal`, the session root and URL opener |
| `aarch64-windows` | 128 | 239 | reference set, `wow64`/`wow64win`, every program but the four left out, their import closure (17 more DLLs and `winspool.drv`), FEX `xtajit`, DXMT |
| `i386-windows` | 624 | 767 | all production modules: + 100 programs and `chcp.com`, `.sys`, `.cpl`, `.ocx`, `.acm`, `.ax`, `.tlb`, theme; no 16-bit modules |

Left out: `winetest.exe`, `control.exe` (mshtml's Gecko installer would wait
forever for a hidden dialog), `winedbg.exe` (see Portal 2 below) and
`winemenubuilder.exe`. `pp registry`: the seed goes from 1,885 to 6,899 keys,
176 to 589 CLSIDs; no key in the old seed is removed and no value changes. Only
BITS (`qmgr`) and WIA (`wiaservc`) register as `LocalService`, which the app
does not run. `http`/`https` gain `URL Protocol` from urlmon; their
`shell\open\command` remains the URL opener.

## Plays

Settings as saved on each game's page; `--until first-frame+10` (Valheim +20).

| Run | IPA | Title | First frame | JIT | Pool head / tail live / room, images | Band used / other |
| --- | --- | --- | ---: | ---: | --- | --- |
| `20261008T174231` | `cdbeaf33` | Hollow Knight (Steam 367520) | 11.48 s | 2.98 s | 81 / 145 / 271 MB, 57 | 3,427 / 375 MB |
| `20261008T180701` | `4a738948` | Hollow Knight | 10.21 s | 2.54 s | 82 / 145 / 270 MB, 57 | 3,491 / 423 MB |
| `20261008T174323` | `cdbeaf33` | Portal 2 (Steam 620) | 6.02 s | 2.63 s | 26 / 161 / 326 MB, 18 | 1,835 / 183 MB |
| `20261008T180752` | `4a738948` | Portal 2 | 5.72 s | 2.46 s | 26 / 161 / 326 MB, 18 | 1,757 / 236 MB |
| `20261008T180857` | `4a738948` | Valheim (Steam 892970) | 7.31 s | 2.52 s | 83 images, room 249 MB, nothing exhausted | 2,689 MB |

- **Hollow Knight** (ARM64EC): same 57 pool images; one new module mapped,
  `tzres.dll` (time zone names, a resource image with no pool copy). The pool
  head grows by 1 MB, the band by 64 MB. Game-local misses unchanged
  (`AMDUnityPlugin`, `NVUnityPlugin`, `openvr_api_dxvk`, `wineopenxr`).
- **Portal 2** (WoW64, Vulkan): identical pool use. On the intermediate IPA
  `d5e26d64` (which still staged `winedbg.exe`) one play of five,
  `20261008T175727`, ended with `exit=0xc0000005` 1.8 s after its first frame:
  `i386 exception c0000005 at eip 78211677`, a NULL read inside DXVK's i386
  `d3d9.dll` (loaded at `780b0000`), then `winedbg --auto 40 880` started and
  failed to create its "Program Error" dialog (`NtUserCreateWindowEx … #32770 …
  failed, error 6`). The game process had loaded no module that the baseline did
  not, so the crash is not attributed to the change, but no earlier play of
  Portal 2 on 2026-10-08 crashed this way (15 runs); the other four plays on
  `d5e26d64` and the one on `4a738948` reached first frame +10. `winedbg.exe` was
  removed from the set so a crash ends the title as before, not through a crash
  dialog the phone cannot show.
- **Valheim** (ARM64EC): first frame and 20 s on; no Wine module missing in its
  log. The PlayFab/XCurl path the survey named was not reached in 20 s.

Screenshots (`screen-stop.png` in each run directory) were taken; they stay in
`.work`. Snakebird Complete (Epic) was not played: it has no Steam ID for the
driver's `--play` and the session was unattended.

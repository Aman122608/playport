# OpenGL on GoldSrc: Counter-Strike and Half-Life reach a map (OpenGL chunk 4)

**Date:** 2026-10-11. **Kind:** phone runs (dev app), host tests.
**Linear:** [PLA-162](https://linear.app/playportdev/issue/PLA-162) (chunk 4);
titles [PLA-22](https://linear.app/playportdev/issue/PLA-22) (Counter-Strike),
[PLA-25](https://linear.app/playportdev/issue/PLA-25) (Half-Life),
[PLA-31](https://linear.app/playportdev/issue/PLA-31), [PLA-32](https://linear.app/playportdev/issue/PLA-32);
the earlier `-soft` fault [PLA-164](https://linear.app/playportdev/issue/PLA-164).
Route: decisions [0087](../decisions/0087-opengl-through-zink-on-the-host.md),
[0088](../decisions/0088-opengl-window-surfaces-and-pacing.md),
[0089](../decisions/0089-opengl-i386-route-gate.md).
**Build:** `patches/madeira-unix` 0001–0117, `patches/wine-unix` 0001–0029, `patches/mesa` 0001–0028.
**IPA:** `Playport-26.5-7c636f4f.ipa` (dev), sha256
`7c636f4fed7c27886d3e476d62797d7654c53139002d6a9e7c8a8d902706147a`, built from the content of the
commit that adds `patches/madeira-unix` 0117 and installed in place. Every run below after the first
is on it. Run directories are under `$PLAYPORT_BUILD/ui-runs/` and `$PLAYPORT_BUILD/perf-runs/`.

## Summary

- **Counter-Strike (Steam 10), i386 GoldSrc on its OpenGL renderer, reaches de_dust2 and holds the
  60 FPS frame limit:** 59.7 FPS mean over 180 s, GPU 3.1 ms a frame.
- **Half-Life (Steam 70) reaches c1a0** (the tram ride) at 60 FPS once loaded, with 12–15 ms of GPU
  time a frame.
- Before the fix below, Counter-Strike drew its first frame and ended 0.5 s later with `c0000005`.
  Wine's i386 `usp10.dll` could not load, and the game then called a null function. That was not an
  OpenGL problem: `patches/madeira-unix` 0117 fixes the loader.
- The i386 GL path itself needed no change. There were no `err:opengl` lines and no
  `outside the WoW64` lines.

## The failure, and 0117

First run on the chunk 3 IPA `ddd76f9a…`, `20261011T174219`, arguments `-gl +map de_dust2`:

- `[winios] opengl: zink Vulkan 1.4(Apple A19 Pro GPU (MESA_KOSMICKRISP)) (Mesa), core 3.3, compat 3.3`
- `[winios] opengl: window surface for hwnd 0x20070 layer … 640x480 -> … (error 0)`, then
  `GL_SIZES:  r:8 g:8 b:8 a:8 depth:24` from the game and `title: +5.31 s first frame`.
- `i386 system call 0028 (…) failed c0000018`, then
  `Loading library USP10.dll (which is needed by L"C:\\Games\\Half-Life\\libcef.dll") failed (error c0000018)`.
  That was followed by `Library libcef.dll (which is needed by L"C:\\Games\\Half-Life\\chromehtml.dll") not found`.
- `[wow64-low-jump] guest RIP=0x0 …`, `i386 exception c0000005 at eip 00000000`, and
  `session: title 1: ended, exit code c0000005 after 882 ms`.

Wine's i386 `usp10.dll` holds only forwarders to gdi32: 0x2000 bytes, preferred base 0x10000000,
an export table, no code and no relocation directory. It is the same in Proton's and the
distribution's Wine. The game's `Mss32.dll` already sat at 0x10000000. Playport's placement of i386
images in the WoW64 window (`patches/madeira-unix` 0055) found usp10 another address. Its
relocation step then refused the image, because the image had no relocation directory
(`STATUS_CONFLICTING_ADDRESSES`). Desktop Wine moves such an image, since it has nothing to fix.

**0117** moves an image with no relocation directory when the server found it holds no code
(`contains_code` clear). An image that has code but no relocation directory is still refused. The
host test `build/wow64/placement_test.c` covers both cases.

After 0117 (`20261011T175032`), the log shows
`[wow64-placement] preferred=0x10000000 guest=0x18ff0000 delta=0x8ff0000 … fixups=guest` and
`i386 image C:\windows\system32\USP10.dll at 18ff0000`. libcef and chromehtml load, and the game goes on:
`Server module initialized.`, `World module initialized.`, `couldn't exec maps/de_dust2_load.cfg`.

## Counter-Strike (Steam 10)

- **Installed** with `install:10` (`20261011T174045`): the Half-Life folder, 883 MB.
- **Settings:** `{"arguments":"-gl +map de_dust2"}`. Steam's launch entry adds
  `-steam -game cstrike` itself. Proton's `default_compat_config()` (Proton 11.0 and Experimental)
  has no entry for app 10 or 70. Every other setting is the default (screen 720, phone aspect,
  limit 60).
- **Prerequisite:** VC++ 2019 x86 runs again for the new build and exits `0x666` (1638, "a newer
  version is installed"). It is recorded as a failure, Playport restarts and the game starts
  (decision 0084).
- **`20261011T175032`, `--until first-frame+30 --shot`:** ok. The game started at +4.82 s and drew
  its first frame at +5.19 s. The screenshot at first frame +30 s shows de_dust2 from a
  spawn: textured stone walls and arches, crates, and the HUD's green buy-zone cart.
- **`pp perf`, `20261011T175225`:** 180 s on the map, frame limit 60, no input. The table below is
  in 5 s buckets; the first bucket includes loading. Thermal state stayed nominal, on the charger.

  | fps mean / median / p10 / min | frame time avg | GPU avg | CPU | server requests |
  |---|---|---|---|---|
  | 59.7 / 59.9 / 59.2 / 58.3 | 16.74 ms | 3.08 ms | 76–89 %, almost all on the E cluster at about 1.6 GHz | 69 per frame |

  Hitches: 787 frames over 25 ms, 14 over 50 ms and 2 over 100 ms, out of 10,780.
  The screenshot at first frame +150 s shows the same view, with the Metal HUD reading 58.5 FPS and 3.10 ms GPU.
- **Uninstalled** through the UI (`uninstall:app-10`, in `20261011T175903`).

**`-soft` (software renderer), checked once on the fixed IPA** (`20261011T175656`). It still fails,
and it is a different failure from the GL path:

- `MessageBoxIndirectW [msgbox] … caption L"Video mode change failure" text L"The specified video mode is not supported.\nThe game will now run in gl mode."`
- `NoExec instruction in entry block: 7A411FD0`, then a repeated
  `i386 exception c0000005 at eip 7a411fd0 (00000008 7a411fd0)`. That is an execute fault at an
  address in no mapped image.
- `[mach_exc] UNHANDLED #1 pc=0x128a5c370 addr=0x0 …`, then
  `[redeliv] 2000 identical redeliveries pc=0x128a5c370 addr=0x0 — unrecoverable host fault misdelivered to guest`
  and `[redeliv] terminating process`.

That is PLA-164's area (the software path's mode change and FEX's fault delivery). It is not
needed now: GL is GoldSrc's default renderer, and it works.

## Half-Life (Steam 70)

- **Install:** `install:70` was refused while Counter-Strike was installed:
  `Games/Half-Life already exists and is not this app's Playport install`. Both apps install into
  Steam's `Half-Life` folder. After Counter-Strike was uninstalled, it installed (590 MB,
  `20261011T175903`).
- **Ownership:** the Downloads page lists the install as "app 70", with no name. Its log has
  `ClientGetAppOwnershipTicket 70: EResult AccessDenied`. The depots came down, and the game runs
  with the emulated Steam API.
- **First Play** (`20261011T180009`): refused with the Cloud save conflict screen (32 files). From
  the game's page, "keep Steam's" was chosen (`20261011T180151`), so nothing was uploaded. That run
  ended at its last action, 6 s after the first frame.
- **`20261011T180314`, `-gl +map c1a0`, `--until first-frame+30 --shot`:** ok. The game started at
  +4.54 s and drew its first frame at +4.89 s. The screenshot shows the tram corridor in
  c1a0, with Black Mesa's lobby and world-map screen ahead.
- **`pp perf`, `20261011T180440`:** 90 s. The first 10 s run at 15 FPS during the startup video and
  map load; after that, 60 FPS (median 60.0, mean 56.9 including the load). GPU time is 12–15 ms a
  frame, and the CPU is mostly on the P cluster (70–98 %). Thermal state went from nominal to fair.
- **Uninstalled** through the UI (in `20261011T180815`).

## The proxies: not owned

- **Geometry Dash** (Steam 322170) and **The Binding of Isaac: Rebirth** (Steam 250900) are not on
  the test account. `install:` stops with `not found: no installable Windows depot for app …`, and
  Downloads lists them as "app 322170" and "app 250900", with no name (`20261011T180815`,
  `20261011T180911`).
- Both queue entries were cancelled in Downloads (`20261011T181108`).

## Direct3D titles on the same IPA

- **Hollow Knight** (`app-367520`, DXMT, `20261011T181343`): `--until first-frame+10 --shot` ok,
  first frame at +10.50 s, the main menu on the screenshot.
- **Portal 2** (`app-620`, i386, DXVK, `20261011T181439`): `--until first-frame+10 --shot` ok, first
  frame at +6.21 s, the "powered by Source" intro on the screenshot. Its log has 85 i386 image
  placements, and the game draws.
- Neither log has an EGL or Zink line.

## Not covered

- **No input was driven.** Both games were only watched on a map, so nothing here shows mouse,
  keyboard or controller play.
- **Half-Life's GPU time** (12–15 ms a frame in c1a0, against 3 ms in de_dust2) leaves little
  headroom at 60 FPS. What costs it (Zink, KosmicKrisp, or the scene) was not measured.
- **The per-call cost on a real engine:** decision 0089 measured 220 ns per i386 call. Counter-Strike
  holds 60 FPS on the E cluster, but the GL calls per frame were not counted.
- **Chunk 0's "not supported yet" message** was never built (the plan skipped chunk 0), so there
  is nothing to remove.

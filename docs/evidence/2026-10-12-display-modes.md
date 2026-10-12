# Evidence: display mode emulation on the virtual monitor; Counter-Strike `-soft` runs full screen (PLA-197)

**Date:** 2026-10-12. **Kind:** phone runs (dev app), log analysis, host tests.
**Linear:** [PLA-197](https://linear.app/playportdev/issue/PLA-197); titles
[PLA-22](https://linear.app/playportdev/issue/PLA-22) (Counter-Strike),
[PLA-25](https://linear.app/playportdev/issue/PLA-25) (Half-Life, the same install folder).
Follows [2026-10-12 GoldSrc `-soft`](2026-10-12-goldsrc-soft-host-fault.md), item 3.
**Changes:**
- `patches/madeira-unix` 0125: display modes on the virtual monitor (commit 6fd94a1);
- `patches/mesa` 0034 and `patches/madeira-unix` 0126 (commit 597d663);
- [decision 0097](../decisions/0097-display-mode-emulation.md), which amends 0088.

The owner was away, so the conservative option was taken at each choice below.
**IPA:** `Playport-26.5-8221f31b.ipa` (dev), sha256
`8221f31b42b5a77f9ece38b0921ada25ebefbb19760ec8f3dbe341c067e61a8b`, built from 597d663 and installed
in place. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

## Summary

- **Root cause.** The iOS port registers no display source; win32u synthesizes one virtual
  monitor, `\\.\DISPLAY1`. A `ChangeDisplaySettings` naming it failed with `BADPARAM`
  (`find_source FAILED`). GoldSrc's software renderer asks for 640x480 by name, first at 16 bpp
  and then at 32. It then gave up, and its fallback faulted (the earlier evidence).
- **madeira-unix 0125** emulates modes as Wine's mode emulation does. The mode list is 640x480,
  800x600, Wine's common sizes that fit in the native size, and the native size, at 32, 16 and
  8 bpp. A listed mode named on `\\.\DISPLAY1` or the primary becomes current: the monitor rect,
  `SM_CXSCREEN` and `ENUM_CURRENT_SETTINGS` report it, the desktop window is resized and
  `WM_DISPLAYCHANGE` is broadcast. The registry mode stays native.
- **Two OpenGL faults behind it.** With the change accepted, GoldSrc (ddraw on wined3d's OpenGL
  renderer, through Zink) resized its window from 1564x720 to 640x480. Two faults followed, each
  fixed below:
  1. Zink faulted at the next copy from the window's buffer (mesa 0034).
  2. The picture then filled only the corner of a full-screen swap chain (madeira-unix 0126).
- **Result.** Counter-Strike `-soft -full +map de_dust2` reaches first frame +30 s in de_dust2,
  full screen, 4:3, with bars at the sides. The gates are unchanged: Counter-Strike `-gl`,
  Hollow Knight and Portal 2 first frame, and Portal 2's `pp perf`.

## 1. The mode change (madeira-unix 0125)

On `8221f31b…`, run `20261012T103545` (`-soft -full +map de_dust2`):

```
[iOS NtUserChangeDisplaySettings ENTRY] devname='\\.\DISPLAY1' … flags=0x4
[iOS NtUserChangeDisplaySettings ENTRY]   mode: 640x480 @0Hz bpp=16 fields=0x1c0000
[vmode] display mode 640x480 16bpp (native 1564x720)
[desktop] 0x10020 server rect (0,0)-(640,480) (set)
… mode: 640x480 @120Hz bpp=32 … → [vmode] display mode 640x480 32bpp (native 1564x720)
… mode: 640x480 @0Hz bpp=16  … → [vmode] display mode 640x480 16bpp (native 1564x720)
```

There is no `find_source FAILED` and no "Video mode change failure" message box. The game window
is placed at `vis={0,0,640,480}`.

The choices taken:
- **16 and 8 bpp are listed and accepted.** Upstream win32u's `get_virtual_modes` lists them, and
  GoldSrc asks for 16 first.
- **The frequency is not checked.** The panel presents at its own rate.
- **An unlisted named mode is refused with `BADMODE`.** An unlisted mode for the unnamed primary
  keeps its old answer (successful, unchanged), so Thumper's `CDS_TEST` and similar callers see no
  change.

## 2. Zink and the resized window (mesa 0034)

On the IPA with only 0125, `e531862f…` (run `20261012T095319`), the game's first flip after the
mode change faulted:

```
[mach_exc] sym pc=…MesaGallium`zink_kopper_acquire_submit+0x1c
bt: zink_resource_copy_region <- util_try_blit_via_copy_region <- zink_blit <- tc_batch_execute …
    <- kopperSwapBuffersWithDamage <- dri2_surfaceless_kopper_swap_buffers
insn 0x39400909 (ldrb w9,[x8,#2]); x8 = 0x1a935692158 = images (0x135692300) + 0xffffffff * 0x1a8
```

The fault read `swapchain->images[UINT32_MAX]`: a copy from a window's buffer that had no image
acquired, because the window had a new swap chain after its resize. It is the same pattern as
mesa 0031 (the resolve target). `zink_blit` tried `try_copy_region` before it looked at a source
that is a window's buffer, and `zink_kopper_acquire_readback` used a never-presented buffer as its
own readback without acquiring it. Now:
- a source that is a window's buffer goes through `blit_native`, which takes its readback;
- a readback that is the buffer itself acquires an image first when it has none.

The copy then reads an image whose contents are undefined, as before the resize. On
`ec80ce34…` (run `20261012T100152`) the game reached de_dust2 with no fault.

## 3. The drawable size (madeira-unix 0126)

On `ec80ce34…` the picture filled only the bottom-left corner. It was 640 of 2736 pixels across,
in a swap chain of `chain=2736x1260`. A diagnostic build, `6e7192d4…` (run `20261012T101803`),
showed this sequence:

```
[winios] opengl: hwnd 0x10076 drawable size 640x480 (virtual 640x480, monitor 640x480, raw 0)
[winios] opengl: hwnd 0x10076 swaps at layer drawable size 640x480
[kk-wsi] present … layer=2736x1260 chain=1564x720
[winios] opengl: hwnd 0x10076 swaps at layer drawable size 2736x1260
[kk-wsi] present … layer=2736x1260 chain=2736x1260
```

The flush set the layer's `drawableSize` to the window's 640x480. By the next swap it was the
layer's bounds in pixels, and Mesa makes the `drawableSize` the swap chain's size. What reset it
was not identified (see "What stays open"). 0126 makes a swap set the `drawableSize` to the
window's size when it differs, and log it, at most 8 times. On `8221f31b…` this happens once:

```
[winios] opengl: hwnd 0x2008e swaps with the layer's drawable at 2736x1260, not the window's 640x480: set again
```

After that, every present is `chain=640x480` (38 of 40). The screenshot shows de_dust2 at the
spawn with the buy icon, 4:3 and centred, with black bars at the sides.

## Phone runs on `8221f31b…`

| Run | Title, arguments | Result |
|---|---|---|
| `20261012T103545` | Counter-Strike `app-10`, `-soft -full +map de_dust2`, first-frame+30 | first frame +6.41 s (the D3D9 intro); 3 mode changes accepted; de_dust2 software-rendered, full screen 4:3 with bars at +30 s; no unhandled fault |
| `20261012T103648` | Counter-Strike, `-gl -full +map de_dust2`, first-frame+30 | first frame +5.12 s; no mode change; the GL chain stays 1564x720 as before (39 presents); de_dust2 at +30 s |
| `20261012T103752` | Hollow Knight `app-367520`, first-frame+10 | first frame +8.71 s; main menu; the 5 `UNHANDLED` lines are the same as on the earlier gates (`20261012T093614`) |
| `20261012T103848` | Portal 2 `app-620`, first-frame+10 | first frame +5.78 s; Source intro |

The arguments were set with `--settings`, which lasts only for the run's session, so
Counter-Strike's saved arguments are unchanged.

Portal 2 `pp perf --title app-620 --secs 120 --pad first-frame+30:p2-cold-boot --pad
first-frame+70:p2-walk`, `pp perf --compare … --window 75:120`. The previous row is from
[2026-10-12 JIT alias release](2026-10-12-jit-alias-release.md).

| Run | IPA | FPS | GPU ms | M instructions a frame | CPU % | CPU mW |
| --- | --- | --- | --- | --- | --- | --- |
| `r2/p2perf` (before) | `b1f0278b…` | 60.0 | 15.19 | 58.9 | 123 | 229 |
| `.work/tmp/p2perf-197` | `8221f31b…` | 60.0 | 15.18 | 60.2 | 121 | 237 |

Unchanged within run-to-run noise.

## Other runs

These ran on intermediate IPAs.

- **`20261012T095453` (`e531862f…`), `-soft -windowed`.** It presented only the two D3D9 intro
  frames. The screen was black at +30 s, with no mode change and no fault.
- **`20261012T100905` (`6e7192d4…`), `-soft -full`.** It stalled the same way after the intro.
- **`20261012T101035`.** Because the game was ended during that stall, its next start showed its
  own crash guard: *"Video mode change failure: The game has detected that the previous attempt to
  start in software video mode failed. The game will now run in OpenGL mode."* (`MB_OKCANCEL`).
  The run waited on the dialog until the supervisor stopped it. The next start
  (`20261012T101803`) showed no dialog and went to `-soft` again.

The stall comes before any display call, and its cause was not investigated. It is not part of
this change.

## Host

`./pp test --quick` passed with all three patches (series, trailers, names, secrets,
`tools/tests`, the app's C tests). Under desktop Wine the same 640x480 change succeeds (the earlier
evidence). No new host run was made for this record.

## What stays open

- **What resets the game layer's `drawableSize` after the GL window is resized.** 0126 corrects it
  at the swap, so one frame is presented at the layer's size. A Vulkan or DXMT swap chain sets the
  `drawableSize` itself and is not affected.
- **Counter-Strike's intermittent start-up stall after its D3D9 intro.** Seen with `-soft`,
  windowed and full screen, in 2 of 8 runs.
- **Windowed `-soft`.** It was not seen past the intro: it stalled the one time it was tried.

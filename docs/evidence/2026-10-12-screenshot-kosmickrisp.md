# In-game menu Screenshot on KosmicKrisp titles (PLA-186)

**Date:** 2026-10-12. **Phone:** iPhone18,4 (A19 Pro), iOS 27.0, on the charger. The dev IPA
`0521958c4d01a4a28383b2c9497f9ecd80ecf572591bd30bd80741978573554a` (commit 86d91c1) was upgraded
in place. Every run's `installed` event names it.

## Problem

The in-game menu's Screenshot said "No frame to save" for every title presented through
KosmicKrisp. That covers DXVK, vkd3d-proton, and OpenGL through Zink. It was seen on Portal 2
(`ui-runs/20261011T163711`), the `wgl` probe and NWN:EE. The menu pauses the game and reads the
last presented drawable's texture (`PacedMetalLayer.lastPresented`).

- **DXMT:** the layer is not framebuffer-only, so Core Image can read that texture.
- **KosmicKrisp:** Mesa's Metal WSI makes the layer framebuffer-only for a hardware driver
  (`wsi_common_metal.c`: `framebuffer_only = !wsi_device->sw`). At present it blits the
  application's swapchain image into the drawable. Neither Core Image nor a blit may read a
  framebuffer-only texture.

The `[frames]` lines show that KosmicKrisp's drawables do come through `PacedMetalLayer`. So the
frame was there; it just could not be read.

This was never a regression. The KosmicKrisp layer has been framebuffer-only since KosmicKrisp
was brought in. The first run on the new build logged `screenshot: frame 1564x720 not read`
(`ui-runs/20261012T105811`).

**Default renderers.** Portal 2 is 32-bit and starts on Vulkan by default
(`LaunchSettings`: Vulkan for 32-bit and Direct3D 12 games). On this phone Hollow Knight also
starts on Vulkan: its launch line says `graphics: vulkan`. Death's Door (Epic, `graphics: dxmt`)
was therefore the DXMT check.

## Change (zero cost until Screenshot is chosen)

**`patches/mesa` 0035.** `kk_wsi_cmd_blit_image` records the present blit once per acquire. It
now hangs the blit's source texture on the drawable's texture as an Objective-C associated
object, retained, under `@selector(playportReadableSource)`. The source is the swapchain image
the game rendered.

- No GPU work and no copy are added.
- The layer stays framebuffer-only.
- A paused game does not render to that image again, so it still holds the frame on screen.

**App.** `PacedMetalLayer`'s presented handler keeps either the drawable's own texture or, when
that texture is framebuffer-only, the associated source.

KosmicKrisp's source images are not shader-readable (logged details below), and Core Image
refuses them. The menu therefore makes a 2D blit copy of slice 0 when Screenshot is chosen, and
reads that copy. That copy is the only GPU work the change adds. A texture Core Image reads
directly, as DXMT's drawables are, is not copied.

| Title | Texture the menu got (logged) |
| --- | --- |
| Portal 2, DXVK | `1564x720 type=2 format=80 usage=4 storage=0 framebufferOnly=false`: 2D, BGRA8, render target only |
| `wgl`, Zink | `782x360 type=3 format=94 usage=5`: a 2D array, BGR10A2, render target and shader read |
| Death's Door, DXMT | read directly, no copy |

## On the phone

All images below are dev copies in `Documents/Screenshots`, pulled with `pp phone pull` and
looked at here. They are not committed. Each one shows the frame the right way up, with its own
colours.

| Title | Renderer | Run | Result |
| --- | --- | --- | --- |
| Portal 2 (`app-620`) | DXVK | `ui-runs/20261012T110142` (IPA `29e9447d…`, same code but one comment) | `20261012T110222.png` 1564x720: the Portal 2 start screen with Chell and Wheatley and the logo |
| Hollow Knight (`app-367520`) | DXVK | `ui-runs/20261012T111341` | `20261012T111411.png`: the Team Cherry logo on black; "Saved to Photos"; Resume, then Quit, exit 0 |
| Death's Door (Epic) | DXMT | `ui-runs/20261012T111530` | `20261012T111614.png`: the title screen with its menu; "Saved to Photos"; no copy needed |
| `wgl` probe (`dir-wgl`) | OpenGL, Zink | `ui-runs/20261012T111648` | `20261012T111711.png` 782x360: a light yellow triangle on yellow; "Saved to Photos" |

Counter-Strike `-gl` was not run; the `wgl` probe covers the GL path.

The quits of Portal 2 and `wgl` through the menu ended with `exit=0x0000042b`. NWN:EE's menu
quit did the same before this change (`ui-runs/20261011T162802`), so it is not caused by this
change.

**Gates.**

- Hollow Knight `--until first-frame+10 --shot`: first frame +10.5 s, the title menu
  (`ui-runs/20261012T111824`).
- Portal 2 `--until first-frame+10 --shot`: first frame +6.3 s, the Source start screen
  (`ui-runs/20261012T111915`).

**Perf.** Portal 2 `pp perf --title app-620 --secs 120 --pad first-frame+30:p2-cold-boot --pad
first-frame+70:p2-walk`, The Cold Boot, window 75-120 s, on the charger:

| Run | IPA | FPS | GPU ms | M instructions a frame (all threads, 75-105 s) |
| --- | --- | --- | --- | --- |
| `perf/pla202-before-1` | `34055093…` | 60.0 | 15.17 | 60.2 |
| `perf/pla186-after-1` | `0521958c…` | 59.9 | 15.18 | 60.1 |

There is no measurable cost.

## Photos permission prompt

On this phone the add-only Photos permission was not granted at the time, although the
2026-09-30 evidence had found it granted. Portal 2's first saved screenshot therefore raised
iOS's "Allow "Playport" to add photos and videos to your photo library?" prompt.

- The prompt stayed up over the Home Screen after the game quit.
- The next driven play (`ui-runs/20261012T110322`) hung behind it until its timeout.
- The tooling cannot answer a system alert. A person tapped Allow; the runs after that saved to
  Photos.

The dev copy in `Documents/Screenshots` is written before Photos is asked, so it is there either
way.

## Open

- KosmicKrisp's source texture keeps the frame only while the game does not render to that
  swapchain image again. A paused game does not, but a swapchain the game destroys between its
  last present and the menu's Screenshot leaves a texture whose memory KosmicKrisp may have freed.
  This was not exercised.
- Any driven run that takes a screenshot on a phone without the Photos permission stops on the
  system prompt.

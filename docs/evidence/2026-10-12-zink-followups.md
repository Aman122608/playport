# Evidence: Zink follow-ups: windowed GoldSrc and the MSAA rebind share one Zink fault (PLA-198, PLA-184)

**Date:** 2026-10-12. **Kind:** phone runs (dev app), log analysis.
**Linear:** [PLA-198](https://linear.app/playportdev/issue/PLA-198) (windowed GL GoldSrc hang),
[PLA-184](https://linear.app/playportdev/issue/PLA-184) (MSAA context rebound crash); titles
[PLA-22](https://linear.app/playportdev/issue/PLA-22) (Counter-Strike).
Route: decisions [0087](../decisions/0087-opengl-through-zink-on-the-host.md),
[0088](../decisions/0088-opengl-window-surfaces-and-pacing.md).
**Change:** `patches/mesa` 0031. The owner was away, so the conservative option was taken at each
choice below.
**IPA:** `Playport-26.5-acba06d2.ipa` (dev), sha256
`acba06d262398d3fda8beabaa533cc5491b117053ad538a56e9d815a540e729c`, built from the content of the
commit that adds `patches/mesa` 0031 and installed in place. Every run below except the first is on
it. Run directories are under `$PLAYPORT_BUILD/ui-runs/` and `$PLAYPORT_BUILD/perf-runs/`.

## Summary

- **One Zink bug behind both issues.** Zink's `begin_rendering`, on its inlined-resolve path (a
  multisampled framebuffer resolved into the window's swap-chain image), marked the swap-chain
  image used by the batch *before* acquiring it. With a new swap chain and no image acquired yet,
  `dt_idx` was still `UINT32_MAX`, and `zink_kopper_acquire_submit` read
  `swapchain->images[0xffffffff]`: a wild read, at the same instruction in both cases.
- **PLA-184 (the `wgl` probe, `10 msaa`):** the 4x context made current again after the second
  window had held the layer crashed at its next swap. Fixed: every check but `border` passes and the
  probe ends normally.
- **PLA-198 (windowed Counter-Strike on GL):** the same fault happened inside `wglSwapBuffers`.
  Delivered to the game, it did not end the process; the next `SwapBuffers` then waited for
  `winios_gl_layer_lock`, which the faulting swap still held: the "host mutex wait" and the black
  screen. Fixed: windowed Counter-Strike reaches its menu and de_dust2.
- **`patches/mesa` 0031** acquires a swap-chain resolve target before the batch uses it, as the
  colour attachments' path (`prep_fb_attachment`) already did.
- Gates on `acba06d2…`: Hollow Knight, Portal 2 and Counter-Strike (`-gl -full +map de_dust2`) reach
  their first frame; Portal 2's `pp perf` is unchanged (15.19 GPU ms, 60.6 M instructions a frame).

## PLA-198: re-test first, then the mutex

The issue predates `patches/madeira-unix` 0121 (the `virtual_mutex` / `ios_wow64_windows_lock`
cycle), so it was re-run first on the installed IPA `88613c10…` (8fa6792, which has 0120 and 0121):

- `20261012T071901`, `-gl -windowed +map de_dust2`, `--until first-frame+40 --shot`: first frame
  +5.65 s (the 640×480 window's surface), then black. The screenshot at first frame +40 s is black.
- 0120's sampler names the mutex. The game's main thread (`tid64=0x4f24b8`) waits in
  `__psynch_mutexwait` on `S1Probe`winios_gl_layer_lock`, and the owner is the same thread
  (`owner_tid64=0x4f24b8`). The frame-pointer chain is `winios_gl_drawable_swap` ←
  `win32u_wglSwapBuffers` ← `wow64_wgl_wglSwapBuffers` ← `__wine_unix_call_dispatcher`. So 0121 had
  not fixed it: a thread holding a non-recursive lock asked for it again.
- Just before that, the log has a host fault in Mesa on the same thread:
  `[mach_exc] UNHANDLED #1 pc=… addr=0x1a97e85215a`, symbolised `zink_kopper_acquire_submit+0x1c`,
  with the backtrace `zink_batch_rp` ← `zink_draw` ← `tc_batch_execute` ← `_tc_sync` ← `tc_flush` ←
  `st_context_flush` ← `dri_flush` ← `kopperSwapBuffersWithDamage` ←
  `dri2_surfaceless_kopper_swap_buffers` ← `dri2_swap_buffers`. No TEB owned the host stack, so
  the fault was delivered best-effort to the guest, which carried on. The swap never returned to
  `pthread_mutex_unlock`, and the next swap blocked on the lock it had left held.
- The faulting instructions (`insn_stream b9449908 5280350a 9baa2508 [39400909] …`) are
  `ldr w8, [dt_idx]`, `mov w10, #0x1a8` (the image record's size), `umaddl x8, w8, w10, x9`,
  `ldrb w9, [x8, #2]` (`dt_has_data`). `x8 - x9 = 0x1a7fffffe58 = 0xffffffff × 0x1a8`: the image index
  was `UINT32_MAX`, the "nothing acquired" value.
- The PLA-184 crash (`20261011T151955`, IPA `2e27a299…`) has the identical instruction stream and
  the same backtrace from `zink_batch_rp` down. One fault, two symptoms.

The only caller on that path is `zink_batch_resource_usage_set`, which takes the batch's acquire
semaphore for a swap-chain image (`zink_kopper_acquire_submit`). In `begin_rendering`, the
inlined-resolve loop (`ctx->dynamic_fb.tc_info.has_resolve`: a multisampled render resolved straight
into the window) called it before `zink_kopper_acquire` for a swap-chain resolve target. After the
window got a new swap chain (the MSAA probe's hand-back; Counter-Strike's window moved to the centre
of the screen, `[win-pos] #30`), no image had been acquired yet. Full screen and single-sample
windows draw to the swap-chain image as a colour attachment, which `prep_fb_attachment` acquires
first, so they never took this path.

**`patches/mesa` 0031** moves the acquire before `zink_batch_resource_usage_set` in that loop.
A failed acquire (a dead swap chain) still ends `begin_rendering` with no render pass, as before.

## Results on `acba06d2…`

| Run | Title, settings | Result |
|---|---|---|
| `20261012T072639` | `dir-wgl`, `10 msaa`, `--until first-frame+35 --shot` | first frame +3.50 s; `msaa: ok format 148, GL_SAMPLES 4, an edge pixel 128`, `second-window: ok`, **`hand-back: ok the first window draws again after the second: 0,0,255`**, the scene at 60.1 fps, `done: 601 frames in 10.0 s, 60.1 fps, 1 failed`; the one failure is `border` (custom border colours are off by default, decision 0088). No host fault; the probe exits 1 for the border check, as on every run without the border option |
| `20261012T072746` | Counter-Strike (app 10), `-gl -windowed +map de_dust2`, `--until first-frame+40 --shot` | **ok**: first frame +5.50 s, `STEAM Auth Server`, `couldn't exec maps/de_dust2_load.cfg`; the screenshot shows de_dust2 (stone arches, crates, the buy-zone cart) in the 640×480 window, aspect-fitted. No `mutexwait`, no host fault |
| `20261012T072929` | Counter-Strike, `-gl -windowed` | **ok**: first frame +5.06 s, `Playing Startup Videos...`; the screenshot shows the main menu (New game, Find servers, Options, Quit) |
| `20261012T073246` | Counter-Strike, `-gl -full +map de_dust2`, `--until first-frame+30 --shot` | **ok**: first frame +5.07 s, the window 1564×720 again (`[win-pos] … vis={0,0,1564,720}`); de_dust2 on the screenshot. This put the game's saved mode back to full screen |

## Gates on `acba06d2…`

- Hollow Knight (`app-367520`), `20261012T073108`: first frame +9.99 s, ok at first frame +10; the
  screenshot shows the title menu.
- Portal 2 (`app-620`), `20261012T073203`: first frame +5.90 s, ok at first frame +10; the "powered
  by Source" intro on the screenshot.
- Counter-Strike on OpenGL, `20261012T073246`: above.
- Portal 2 `pp perf --title app-620 --secs 120 --pad first-frame+30:p2-cold-boot --pad
  first-frame+70:p2-walk` (`perf-runs/sweep08-p2-base`), `pp perf --compare … --window 75:120`
  against the last baseline:

  | Run | IPA | FPS | GPU ms | M instructions a frame | CPU % | CPU mW |
  | --- | --- | --- | --- | --- | --- | --- |
  | `jitalias/r2/p2perf` | `b1f0278b…` | 60.0 | 15.19 | 58.9 | 123 | 229 |
  | `sweep08-p2-base` | `acba06d2…` | 60.0 | 15.19 | 60.6 | 122 | 238 |

  Unchanged (Direct3D through DXVK takes no resolve into a Zink swap chain).

## Not covered

- The window system's lock is still held across `eglSwapBuffers`, so any other host fault inside
  Mesa that is delivered to the game would wedge its GL the same way. No other such fault is known.
- `patches/mesa` 0031 is not offered upstream yet.

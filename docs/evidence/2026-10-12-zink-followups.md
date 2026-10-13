# Evidence: Zink follow-ups: one Zink fault behind windowed GoldSrc and the MSAA rebind; border colours; GoldSrc's GPU time (PLA-198, PLA-184, PLA-181, PLA-189, PLA-191)

**Date:** 2026-10-12. **Kind:** phone runs (dev app), log analysis.
**Linear:** [PLA-198](https://linear.app/playportdev/issue/PLA-198) (windowed GL GoldSrc hang),
[PLA-184](https://linear.app/playportdev/issue/PLA-184) (MSAA context rebound crash),
[PLA-181](https://linear.app/playportdev/issue/PLA-181) (custom border colours),
[PLA-189](https://linear.app/playportdev/issue/PLA-189) (i386 persistent maps),
[PLA-191](https://linear.app/playportdev/issue/PLA-191) (Half-Life's GPU time); titles
[PLA-22](https://linear.app/playportdev/issue/PLA-22) (Counter-Strike),
[PLA-25](https://linear.app/playportdev/issue/PLA-25) (Half-Life).
Route: decisions [0087](../decisions/0087-opengl-through-zink-on-the-host.md),
[0088](../decisions/0088-opengl-window-surfaces-and-pacing.md),
[0093](../decisions/0093-opengl-border-colours-and-fans-on-kosmickrisp.md) (new).
**Changes:** `patches/mesa` 0031, 0032 and 0033. The owner was away, so the conservative option was
taken at each choice below.
**IPAs (dev), installed in place:**
- `Playport-26.5-acba06d2.ipa`, sha256
  `acba06d262398d3fda8beabaa533cc5491b117053ad538a56e9d815a540e729c`: the content of commit d506e4a
  (0031). The PLA-198 and PLA-184 runs and the first gates are on it.
- **`Playport-26.5-673566a7.ipa`, sha256
  `673566a7ae4619ae71c5e82d210bb7d116600e967552e8df045496e3f4349eff`**: the content of the commit
  that adds 0032 and 0033, the final IPA. Its gates and probe runs are below.

Run directories are under `$PLAYPORT_BUILD/ui-runs/`, `$PLAYPORT_BUILD/perf-runs/` and
`$PLAYPORT_BUILD/gpu-runs/`.

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
- **PLA-181: custom border colours are on for Zink, and only for Zink** (`patches/mesa` 0032,
  decision 0093). With them on, the `wgl` probe reads the right `GL_CLAMP_TO_BORDER` colour. Portal 2
  (DXVK) with them on, through the per-game option, has the same steady GPU time. They are still off
  for DXVK and vkd3d-proton.
- **PLA-191: Half-Life's GPU time came from KosmicKrisp's triangle-fan unrolling** (fixed:
  `patches/mesa` 0033, decision 0093). Metal has no fans, so every fan draw ran a compute pass first.
  That ended the render pass, 68 times in a c1a0 frame. With Zink converting fans on the CPU, c1a0
  takes **1.59 ms** of GPU time a frame (from 15.28 ms), and de_dust2 1.57 ms (from 3.08 ms).
- **PLA-189: not implemented.** Placing Zink's maps inside the WoW64 window is not contained. Its
  design is written down below as the next step.
- Gates on the final `673566a7…`: Hollow Knight, Portal 2 and Counter-Strike
  (`-gl -full +map de_dust2`) reach their first frame. Portal 2's `pp perf` is unchanged (15.2 GPU ms,
  60.8 M instructions a frame).

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

## Gates on `acba06d2…` (0031 only)

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

## PLA-181: custom border colours (`patches/mesa` 0032)

KosmicKrisp has `VK_EXT_custom_border_color` only behind `KK_EXPERIMENTAL(CUSTOM_BORDER)`. The
`wgl` probe's `border` check samples a texture with `GL_CLAMP_TO_BORDER` and a blue border outside
it, and reads the pixel back.

| Run | IPA | Settings | Result |
|---|---|---|---|
| `20261012T072639` | `acba06d2…` | `10 msaa` | `border: FAIL GL_CLAMP_TO_BORDER blue reads 0,0,0,0`; Zink warns `have_EXT_custom_border_color` and `customBorderColorWithoutFormat` missing |
| `20261012T074026` | `acba06d2…` | `20`, Graphics option `MESA_KK_EXPERIMENTAL=custom_border` | `border: ok GL_CLAMP_TO_BORDER blue reads 0,0,255,255`; `done: 1201 frames in 20.0 s, 60.0 fps, 0 failed` |
| `20261012T075329` | `a0179ce1…` (0032 without 0033) | `20`, no option | `border: ok … 0,0,255,255`, `0 failed`; neither Zink warning is in the log |
| `20261012T082003`, `20261012T082106` | `673566a7…` | `20`; `10 msaa` | every check ok, `border` included, `0 failed` |

Turning the flag on for every process would change every Vulkan title. Measured on Portal 2 (DXVK,
which enables the feature when the device has it) with the per-game option, `--window 75:120`:

| Run | IPA | GPU ms | M instructions a frame | system mW | hitches > 50 ms |
|---|---|---|---|---|---|
| `perf-runs/sweep08-p2-base` | `acba06d2…` | 15.19 | 60.6 | 3553 | 0 |
| `perf-runs/sweep08-p2-border` (first launch with it) | `acba06d2…` | 11.71 | 61.6 | 4063 | 1 |
| `perf-runs/sweep08-p2-border2` (second) | `acba06d2…` | 15.21 | 58.7 | 3277 | 0 |

- The first launch with the flag recompiled its shaders (51 Metal compiles over 20 ms, against none
  in the baseline), because the shader key includes the feature. That launch had a hitch over 100 ms,
  and its GPU figures are not comparable (it drew more power). The second launch matched the baseline.
- The conservative choice was to turn custom border colours on for **Zink only**. DXVK and
  vkd3d-proton titles would all recompile their shaders once. No Direct3D title on the phone is known
  to need custom border colours. KosmicKrisp upstream still calls the feature experimental.
- 0032 gives both extensions to an instance whose `VkApplicationInfo` engine is `mesa zink`. Zink
  sets that name, and no other client does. The flag still turns them on for every instance, as
  before. Decision 0093 records this, amending 0088.

## PLA-191: Half-Life's GPU time (`patches/mesa` 0033)

Half-Life (Steam 70) was installed for this through the UI (`install:70`, `20261012T075458`). It
joined Counter-Strike's folder (decision 0092) and was ready after 6 s.

**Measured** (`pp perf --title app-70 --secs 90 --settings '{"arguments":"-gl +map c1a0"}'`, the tram
ride; Counter-Strike `-gl -full +map de_dust2`, the spawn; `--window 40:90`):

| Run | IPA | FPS | GPU ms | GPU % | M instructions a frame | CPU % | system mW |
|---|---|---|---|---|---|---|---|
| `perf-runs/sweep08-hl-c1a0` | `a0179ce1…` (0031, 0032) | 59.6 | **15.28** | 91 | 124.2 | 98 | 4661 |
| `perf-runs/sweep08-hl-nofans` | `673566a7…` (with 0033) | 60.0 | **1.59** | 18 | 100.7 | 91 | 2845 |
| `perf-runs/sweep08-cs-final` | `673566a7…` | 59.8 | **1.57** | 17 | 41.9 | 76 | 1585 |

Counter-Strike's earlier figure was 3.08 GPU ms over 180 s (evidence 2026-10-11-opengl-goldsrc).

**What the GPU did.** `pp gpu capture --frame 1800` of each, on `a0179ce1…`
(`gpu-runs/sweep08-hl-cap`, `gpu-runs/sweep08-cs-cap`; `capture.txt`):

- Half-Life, c1a0: 3 command buffers, **136 passes**, 1,157 draws. After the first render pass
  (418 draws), the frame alternates a compute pass (two dispatches, the same two pipelines each time)
  with a 640×480 render pass that loads and stores its RGBA8 colour and D32F depth. Each of those
  render passes has 3 to 43 draws, and its first draw is indirect.
- Counter-Strike, de_dust2: 14 passes, 130 draws. Four render passes follow a compute pass, then the
  game's 640×480 image is drawn to the 1564×720 window.

Reading KosmicKrisp's draw path, those compute passes are its unroll (`kk_unroll_geometry`), which
turns a draw Metal cannot do into an indirect indexed triangle list; the draw after it is the
indirect one. The capture does not name the compute functions, so this is the code's reading,
confirmed by the change below. A compute pass cannot run inside a Metal render pass, so each unroll ends the
render pass, and the next one loads the colour and depth targets again. The draws that need it
(`requires_unroll`) are triangle fans, which Metal lacks. GoldSrc draws its studio models
(scientists, the tram) as `GL_TRIANGLE_FAN` and `GL_TRIANGLE_STRIP` commands, and c1a0 has many on
screen, de_dust2's spawn few. Zink passed fans down because KosmicKrisp has no portability subset, so
Zink took fans as supported. With fans and geometry shaders, Zink also drew quads through a
geometry shader, which KosmicKrisp runs as compute too.

**0033** makes Zink report no triangle fans for KosmicKrisp's driver ID. Mesa's gallium frontend then
converts fans and quads to triangle lists on the CPU, as it does for drivers whose portability subset
lacks fans. The CPU work went down, not up: 124 M to 101 M instructions a frame, because the
emulated game no longer waited on render-pass splits. The screenshots at first frame +60 s show the
same c1a0 corridor (the Black Mesa lobby and world-map screen ahead). The Metal HUD reads 15.40 ms of
GPU time before the change and 1.72 ms after.

**Regression checks on `673566a7…`** (one session):

| Run | Probe or title | Result |
|---|---|---|
| `20261012T081712` | `dir-gl21` (x86-64; `GL_QUADS` with read-backs), 20 s | every check ok, `0 checks failed`, 60.0 fps |
| `20261012T081809` | `dir-gl21-i386`, 20 s | every check ok, `0 checks failed` |
| `20261012T081902` | `dir-gl21-i386`, `30 stress`, frame limit Off | immediate **8.39 ms** a frame (draw calls 3.56 ms), client-array 8.40, vbo 8.34; 0 failed. Decision 0089 measured 8.42 / 8.39; its budget is 16 |
| `20261012T082003` | `dir-wgl`, 20 s | every check ok, `border` included; `0 failed` |
| `20261012T082106` | `dir-wgl`, `10 msaa` | every check ok, including `msaa` and `hand-back`; `0 failed` |
| `20261012T082157` | Counter-Strike, `-gl -windowed +map de_dust2` | first frame +5.92 s, `de_dust2_load.cfg`, no host fault, no mutex wait |

**Afterwards:** `uninstall:app-70` (`20261012T083205`): `library: uninstall Half-Life (app-70)`, then
`folder present=true (shared) record present=false`. `--verify app-10` (`20261012T083301`):
`5611/5611 OK, 440 unlisted`. Most of the unlisted files are the two capture bundles
(`kk-frame1800-….gputrace`). KosmicKrisp writes a capture into the game's working directory, here
`C:\Games\Half-Life`, and `pp gpu capture` pulls it without deleting it. That is a follow-up for the
tool. Counter-Strike's own files all verify.

## PLA-189: i386 persistent buffer maps (not implemented; the design)

Today (wine-unix 0029), opengl32's WoW64 `wow64_map_buffer` keeps a driver mapping that happens to
fall inside the caller's WoW64 window ("we're lucky"). Otherwise it copies through a window buffer, and
it refuses `GL_MAP_PERSISTENT_BIT` with an `ERR`. `GL_ARB_buffer_storage` is hidden from i386
contexts.

What "Zink places its maps inside the WoW64 window" needs, from the code at the pin:

- **KosmicKrisp has placed maps.** `VK_EXT_map_memory_placed` is exposed, and `memoryMapPlaced` is
  set (mesa 0003 maps them with the `vm_` calls on iOS). **Zink does not use them.** It maps a BO
  with `vkMapMemory` and keeps that mapping for the BO's life (`bo->u.real.cpu_ptr`).
- **Buffers share BOs.** Zink suballocates small buffers from slabs, so one BO holds buffers of many
  contexts. All the pseudo-processes share one EGL display, and so one Zink screen: the x86-64 parent
  and every i386 child. A slab BO that one i386 child has mapped inside its own window can hold
  another child's buffer, which needs it inside a different window. Placing per buffer means a
  persistent buffer gets its own BO (no slab) and its own placed map.
- **Who chooses the address.** Only the caller's process knows its window: opengl32's unix side, from
  the TEB. Zink would need a hook it calls at map time, for example a screen callback that the
  win32u/opengl32 side installs. It would return an address range inside the calling thread's window,
  reserved with that process's `NtAllocateVirtualMemory` and its zero bits.
- **Lifetime.** A placed range must stay reserved as long as the BO is mapped, and be freed when it
  is unmapped. When a child exits, its window is retired (madeira-unix 0110). Any BO still mapped
  there must be unmapped first, or Zink keeps a pointer into a released window.

Next step, as its own chunk with a probe phase first:

1. Add an i386 `gl21` phase that maps a buffer with `GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT`
   (behind a context that exposes `GL_ARB_buffer_storage`), writes it across frames and draws from
   it. Today it would fail at the map.
2. A mesa patch: a `pipe_screen` hook for a placed-map allocator; for a persistent map, a dedicated
   BO mapped with `VkMemoryMapPlacedInfoEXT` at the address the hook returns.
3. A wine-unix patch: opengl32's WoW64 side installs the hook, reserves in the caller's window, frees
   at unmap and before the window retires; and exposes `GL_ARB_buffer_storage` to i386 again.

No title needs it today: the known i386 GL titles (GoldSrc) use GL 1.x–2.x without buffer storage,
and decision 0089's ceiling is GL 3.3.

## Final gates on `673566a7…`

- Hollow Knight (`app-367520`), `20261012T082312`: first frame +9.36 s, ok at first frame +10; the
  title menu on the screenshot.
- Portal 2 (`app-620`), `20261012T082403`: first frame +6.26 s, ok at first frame +10; the "powered
  by Source" intro on the screenshot.
- Counter-Strike, `-gl -full +map de_dust2`, `20261012T082447`: first frame +5.73 s, ok at first
  frame +30; de_dust2 on the screenshot, in full screen (the game's saved mode is full screen again).
- Portal 2 `pp perf`, The Cold Boot (`perf-runs/sweep08-p2-final`, `--window 75:120`): 60.0 FPS,
  **15.20 GPU ms**, **60.8 M instructions a frame**, CPU 125 %, against 15.19 and 60.6 on
  `acba06d2…`. Unchanged.

## Not covered

- The window system's lock is still held across `eglSwapBuffers`, so any other host fault inside
  Mesa that is delivered to the game would wedge its GL the same way. No other such fault is known.
- `patches/mesa` 0031–0033 are not offered upstream yet.
- No input was driven in Half-Life or Counter-Strike: only the tram ride and a spawn were watched.
- The fan conversion was measured on GoldSrc and the probes only. Neverwinter Nights (the other GL
  title seen) was not replayed: it is not installed.

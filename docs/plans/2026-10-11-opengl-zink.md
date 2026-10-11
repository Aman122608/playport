# Plan: OpenGL through Mesa's Zink over KosmicKrisp

**Linear:** [PLA-162](https://linear.app/playportdev/issue/PLA-162)
(class A of [PLA-12](https://linear.app/playportdev/issue/PLA-12); titles PLA-30, PLA-31,
PLA-32, PLA-22/PLA-25 after PLA-9; follows PLA-107).
**Date:** 2026-10-11. **Kind:** execution plan (run from the compat speedrun's queue,
owner 2026-10-11; branch `dev/pla-152-gdi-windows`). Written from research; chunk results
are in the notes under each chunk.
**Status:** chunks 1a, 1b, 2, 3 and 4 done: the `gl21` and `wgl` probes and Neverwinter Nights:
Enhanced Edition draw through Zink on KosmicKrisp at GL 3.3 compatibility on x86-64, and the i386
`gl21` probe draws through opengl32's WoW64 thunks. The route gate chose host-side for i386
([decision 0087](../decisions/0087-opengl-through-zink-on-the-host.md), amended by
[0088](../decisions/0088-opengl-window-surfaces-and-pacing.md) and
[0089](../decisions/0089-opengl-i386-route-gate.md);
[1a evidence](../evidence/2026-10-11-mesa-gl-ios-build.md),
[1b evidence](../evidence/2026-10-11-opengl-zink-route.md),
[chunk 2 evidence](../evidence/2026-10-11-opengl-wgl-completeness.md),
[chunk 3 evidence](../evidence/2026-10-11-opengl-i386.md),
[chunk 4 evidence](../evidence/2026-10-11-opengl-goldsrc.md)). Chunk 4 done: Counter-Strike and
Half-Life reach a map on GoldSrc's OpenGL renderer. Next: chunk 5 (docs).

**Settled (owner, 2026-10-11):** do Zink, as its own chunk after the compatibility speedrun's
phase 1 and the prefix parity audit (PLA-163). Chunk 0 (a "not supported yet" message) is
skipped: Zink replaces it. Facts are labelled **[tree]** (read in the pinned trees under
the build area's run tree (`$PLAYPORT_BUILD/run`), read-only), **[doc]** (repository docs/patches), **[web]** (external source,
appendix) or **[inference]** (the researcher's reasoning, not yet measured).

## Goal

Windows games that draw with OpenGL (WGL) run on the phone: x86-64 and i386 (WoW64),
fixed function and GLSL, at the GL level those games need (2.0–3.3), presenting into the
app's one game layer with the same first-frame signal, pacing and screenshots DXVK has.
Hollow Knight (DXMT/x86-64) and Portal 2 (DXVK/i386) must not change.

## Where things stand

- **No GL at all.** The iOS display driver's `OpenGLInit` is `nulldrv_OpenGLInit`
  (`STATUS_NOT_IMPLEMENTED`) [doc: `patches/madeira-unix` 0029 context]; `libwin32u_unix.a`
  is compiled with `-USONAME_LIBEGL` [doc: 0029 context of `build/win32u-unix/build.sh`];
  opengl32's unix side is not compiled: opengl32 gets `ios_gl_stub_unix_call_table`, the
  GL-absent table, for x86-64 and (since 0099/PLA-107) WoW64 callers [doc: 0099]. The Mesa
  stage builds `-Dgallium-drivers= -Dopengl=false -Degl=disabled` [doc: `build/stages/mesa.sh`].
- **Wine 11.19 already contains a whole EGL OpenGL backend** [tree:
  `dlls/win32u/opengl.c`, `include/wine/opengl_driver.h`, `WINE_OPENGL_DRIVER_VERSION 41`]:
  - `display_funcs_init` (run once, lazily, from `__wine_get_opengl_driver`) first calls
    `egl_init`, which `dlopen`s `SONAME_LIBEGL`, requires `EGL_KHR_client_get_all_proc_addresses`
    and `EGL_EXT_platform_base`, and sets `driver_funcs = &egldrv_funcs`; **then** calls the
    user driver's `pOpenGLInit(version, &display_funcs, &driver_funcs)`, which may keep or
    replace those funcs; then `init_egl_platforms`.
  - `egldrv_funcs` defaults to `EGL_PLATFORM_SURFACELESS_MESA`, makes contexts with
    `eglCreateContext(EGL_NO_CONFIG_KHR)`, pbuffers with `eglCreatePbufferSurface`, and window
    drawables as `framebuffer_surface`s: the game draws into GL FBOs, and on
    `SwapBuffers` win32u blits (with the gamma ramp) into a **target** driver drawable and
    calls its `swap`. `egldrv_surface_create` passes target `NULL`, so on its own nothing
    reaches a screen.
  - With `emulate_modeset`, win32u wraps *any* driver window drawable in a framebuffer
    surface ("experimental framebuffer OpenGL surface") and scales `virtual_size` to
    `monitor_size`: a 640×480 game on the virtual monitor without a real mode change.
  - `init_device_info` probes core and compatibility versions (4.6 down to 1.0) and logs
    them: a free feature-level report on the phone.
  - The opengl32 WoW64 thunks exist for every GL function [tree:
    `dlls/opengl32/unix_thunks.c`, `unix_wgl.c`], including the new-WoW64 buffer-mapping
    copy (`wow64_map_buffer`) and client-side strings (`opengl_wow64_str`, `version_str`).
- **Mesa can build Zink + EGL for Darwin** [tree: `meson.build`]: `with_dri` is true for
  `gallium + zink + host_machine.system() == 'darwin'` with EGL enabled; EGL then always adds
  the `surfaceless` platform. But at the pin:
  - Zink's meson **errors on Darwin without `-Dmoltenvk-dir`**, and `zink_screen.c` includes
    MoltenVK headers under `__APPLE__` and `dlopen`s `@rpath/libvulkan.1.dylib` (a loader
    Playport does not ship) [tree: `src/gallium/drivers/zink/meson.build`, `zink_screen.c`].
    KosmicKrisp already exports `vkGetInstanceProcAddr` [doc: `patches/mesa` 0008].
  - **Kopper (Zink's WSI) knows only XCB, Wayland and Win32 surfaces**; anything else is
    `UNREACHABLE` [tree: `zink_kopper.c` `init_dt_type`, `kopper_CreateSurface`,
    `find_dt_entry`].
  - EGL surfaceless has **no window surfaces**; without DRM devices it reaches Zink only
    through `surfaceless_probe_device_sw` when `ForceSoftware` and `Zink` are both set
    (`MESA_LOADER_DRIVER_OVERRIDE=zink`) [tree: `platform_surfaceless.c`].
- **KosmicKrisp's features against Zink's list** [tree: `kk_physical_device.c`; web: Zink docs]:
  present: logicOp, fillModeNonSolid (patch 0005), alphaToOne, clip/cull distance,
  maintenance1/2/5, renderpass2, imageless framebuffer, timeline semaphores, provoking vertex,
  swapchain mutable format, descriptor update templates, scalar block layout, transform
  feedback (0006), conditional rendering, independent blend, geometry and tessellation
  shaders, depth clamp/clip enable, dual-source blend, vertex divisor, sample shading, cube
  arrays, multi viewport, image 2D view of 3D, draw-indirect count, anisotropy, depth bias
  clamp, robustness2 with `nullDescriptor`. **Gaps:** `VK_EXT_custom_border_color` and
  `border_color_swizzle` only behind `KK_EXPERIMENTAL(CUSTOM_BORDER)`; line rasterization
  exposes `bresenhamLines` only (no rectangular/smooth/stippled lines); **no
  `shaderFloat64`**; no `VK_KHR_external_memory_fd` (only Metal and host external memory).
- **GL ceiling** [tree: `src/mesa/main/version.c`]: GL 4.0 requires `ARB_gpu_shader_fp64`.
  [inference] With no `shaderFloat64`, Zink on KosmicKrisp reports **GL 3.3** (core and
  compatibility) unless Zink/st turn on soft-fp64 emulation; a public macOS run of Zink on
  KosmicKrisp needed `MESA_GL_VERSION_OVERRIDE=4.6` [web], consistent with that. Every
  reported OpenGL title needs ≤ 3.3 (Isaac, Geometry Dash 2.0; GoldSrc 1.x).
- **Prior art** [web]: Zink on KosmicKrisp runs GL 4.6-class apps on macOS (with the version
  override), after adding Metal window surfaces to EGL surfaceless and a Metal kopper type;
  it hit KosmicKrisp disk-cache reuse across launches, alpha-0 frames shown transparent by
  the compositor, and intermittent black frames from Zink's `vk_meta` format-converting blit.
  A WineHQ draft MR (!10531, opened 2026-04-01, not merged) builds Mesa's WGL+Zink as the
  PE-side `opengl32` over PE `vulkan-1`; its author reports it slower than the unix GL bridge.

## Route decision: host-side Zink, with an i386 measurement gate

**Recommended: host-side** — Mesa's libEGL + Zink built for iOS from the same Mesa pin as
KosmicKrisp, reached through win32u's existing EGL backend and Wine's opengl32 unix thunks,
presenting with a Metal surface on the game layer.

Why (evidence above):

1. **Least new Wine code.** win32u 11.19 already implements WGL on EGL surfaceless
   (contexts, sharing roots, pixel formats from EGL configs, pbuffers, swap interval,
   framebuffer emulation, gamma). The iOS driver adds one `OpenGLInit` that keeps
   `egldrv_funcs` and overrides `p_surface_create` (≈ the size of 0029's `VulkanInit`).
   Wine's PE `opengl32` (x86-64 and i386) stays as shipped.
2. **Native code for both machines.** Mesa's GL frontend and Zink's NIR→SPIR-V run as
   ARM64 in the app; on the guest route the i386 Mesa would run under FEX.
3. **One Mesa build, one pin, one stage** (`mesa.sh` already builds KosmicKrisp from it);
   no ARM64EC/i686 Mesa PE builds (unproven here) and no extra ARM64EC image copied whole
   into the JIT pool (ARCHITECTURE "JIT pool use").
4. **Byproduct:** wined3d (Wine's d3d ≤ 11 and ddraw on GL) starts working for i386, where
   DXMT has no build.

Costs and the open question (why there is a gate):

- **Presentation needs Mesa patches** (Metal kopper surface, EGL window surface over a
  `CAMetalLayer`, no-DRM Zink selection, no MoltenVK headers).
- **i386 pointers.** Playport's WoW64 guest pointers are `B + zext32(p)`, not identity.
  winevulkan needed every `UlongToPtr`/`PtrToUlong` (about 2,100 uses) converted through the
  window (wine-unix 0011) [doc: Portal 2 milestone 2 evidence]. opengl32's generated thunks
  are larger and **ambiguous**: `ULongToPtr` is also used for `GLintptr`/`GLsizeiptr`
  values [tree: `unix_thunks.c`, e.g. `wow64_ext_glMulticastBufferSubDataNV`], and many GL
  "pointers" are **offsets when a buffer object is bound** (vertex/attrib pointers, element
  indices, pixel pack/unpack, indirect draws). A blanket window macro would break them.
- **Per-call cost on i386.** Every GL call from an i386 game is an emulator exit, a WoW64
  thunk and a unix call [inference]; immediate-mode GoldSrc can make tens of thousands per
  frame. The guest route keeps those calls inside the emulated Mesa and only crosses for
  Vulkan.
- **Persistent buffer maps on i386** use Vulkan memory exported as an opaque fd and imported
  into GL [tree: `create_buffer_storage`]; KosmicKrisp has no opaque-fd export, so
  `GL_MAP_PERSISTENT_BIT` under WoW64 hits Wine's `FIXME` [tree: `wow64_map_buffer`]. GL 4.4
  engines on i386 are out until that is reworked (Zink placed maps; KosmicKrisp has
  `EXT_map_memory_placed`).

**Gate (chunk 3):** the i386 `gl21` probe's immediate-mode stress (below) is measured. If
host-side i386 meets its budget, host-side serves both machines. If not, the decision record
names the **guest-side i386 fallback** (Mesa WGL + Zink as an i686 `opengl32.dll` over Wine's
i386 `winevulkan`, whose WoW64 thunks and placed maps already work for DXVK): x86-64 stays
host-side. The decision record is written at the end of chunk 1 (route proven on x86-64) and
amended at chunk 3. *Decided 2026-10-11 ([0089](../decisions/0089-opengl-i386-route-gate.md)):
i386 took 8.42 ms per frame against a 16 ms budget, so host-side serves both machines.*

## Design (host-side)

### Mesa (patches/mesa, `build/stages/mesa.sh`)

- A second iOS configure (or the same tree, second build dir) for GL:
  `-Dgallium-drivers=zink -Dvulkan-drivers= -Dopengl=true -Degl=enabled
  -Degl-native-platform=surfaceless -Dplatforms=macos -Dglx=disabled -Dgles1=disabled
  -Dgles2=disabled -Dgbm=disabled -Dglvnd=disabled -Dllvm=disabled -Dexpat=disabled
  -Dzstd=disabled`, plus the existing cross file and host tools. [inference] Exact options
  settle in chunk 1a; `-Dplatforms=macos` is what `mesa.sh` passes today.
- Patches (each `Class: ios-port` or `feature`, with `Evidence:`):
  1. **Zink on iOS without MoltenVK:** Darwin without `moltenvk-dir` uses Mesa's own Vulkan
     headers; the `MoltenVK/*` includes and `MVK_VERSION` use are compiled out on iOS;
     `VK_LIBNAME` is `@rpath/KosmicKrisp.framework/KosmicKrisp` on iOS (the framework win32u
     already `dlopen`s), entry `vkGetInstanceProcAddr` (patch 0008).
  2. **Kopper Metal surfaces:** `KOPPER_METAL` for `VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT`
     in `init_dt_type`, `kopper_CreateSurface` (`vkCreateMetalSurfaceEXT`), `find_dt_entry`
     (keyed by the layer) and the resize path (surface capabilities, not X11 only).
  3. **EGL window surfaces on surfaceless for a `CAMetalLayer`:** `create_window_surface`,
     `swap_buffers`, `swap_interval` in the surfaceless vtbl when kopper is used; the loader's
     `SetSurfaceCreateInfo` fills `VkMetalSurfaceCreateInfoEXT` with `present_opaque = true`
     (KosmicKrisp 0012 makes an opaque swap chain's layer opaque; prior art shows alpha-0
     frames otherwise) and `GetDrawableInfo` reads the drawable size without touching
     CoreAnimation off the main thread (prior art: `drawableSize` races).
  4. **Zink without DRM:** surfaceless picks `zink` with kopper directly on iOS, without
     `LIBGL_ALWAYS_SOFTWARE`. [inference, to check in `zink_screen.c`] that variable would also
     make Zink prefer a CPU Vulkan device, of which there is none.
  5. **Caches:** Mesa's disk cache path inside the container, or off for Zink until KosmicKrisp's
     cross-launch reuse is checked (prior art saw stale pipeline pointers; Playport's
     `patches/mesa` 0015 changed KosmicKrisp's caches).
- Output: `libEGL` and `libgallium` dylibs (or one, if meson allows linking gallium into EGL)
  as framework(s) beside `KosmicKrisp.framework`, signed (Info.plist, as `stage_framework`),
  `check-macho-imports.py` on each (`-undefined dynamic_lookup` hides misses otherwise), the
  SDK libc++ headers (PLA-5), `-ffile-prefix-map`.
- **Host test** (extends `build/mesa-host-test`): the same tree for Linux, Zink on the
  KosmicKrisp mock: EGL surfaceless context, fixed-function and GLSL 1.20 draws, a
  `glBlitFramebuffer`; it fails on a crash, a NIR validation error or an untranslated MSL
  intrinsic. It proves Zink accepts KosmicKrisp's device and that Zink's shaders translate
  to MSL, without a phone.

### Wine unix side (patches/madeira-unix, patches/wine-unix)

- `build/win32u-unix`: stop `-USONAME_LIBEGL`; `config_ios.h` names the EGL framework.
  `driver_ios.c` gets `winios_OpenGLInit`: copy the `egldrv_funcs` it is handed, keep the
  surfaceless platform, override `p_surface_create` with a drawable whose EGL window surface
  is on the `CAMetalLayer` from `macdrv_view_create_metal_view(hwnd)` (the game layer, or the
  window's own in desktop mode, exactly as `winios_vulkan_surface_create`), `swap` =
  `eglSwapBuffers`, `flush` handles `GL_FLUSH_INTERVAL`/`GL_FLUSH_UPDATED`. The EGL surface is
  made at the window's first make-current, and the game layer is handed between windows
  (decision 0088; 1b made it at the first swap, which lost the first frame).
- `build/ntdll-unix`: compile `dlls/opengl32/unix_wgl.c` and `unix_thunks.c` into
  `libntdll_unix.a` (as 0029 did for winevulkan); `load_builtin_unixlib` hands opengl32 its
  real table and its WoW64 table instead of `ios_gl_stub_unix_call_table`.
- **Nothing loads Mesa until a game asks for GL.** `process_attach` is lazy [tree]; check that
  `get_opengl_gpus` (win32u GPU enumeration) does not reach `__wine_get_opengl_driver` in
  every launch [to check in `sysparams.c`]; if it does, skip it on iOS. Gate: Hollow Knight's
  and Portal 2's logs show no EGL/Zink line.
- One unix side serves every pseudo-process: opengl32's globals (`zero_bits`, `is_wow64()`
  result, buffer tables, win32u's root contexts) are per Mach process. WoW64 decisions must
  come from the calling thread (TEB32 pair), as wine-unix 0011 does for winevulkan.

### Presentation, first frame, pacing

- GL presents through Zink → KosmicKrisp WSI → the same `CAMetalLayer`
  `IOSDisplayShim` hands DXMT and Vulkan (`g_layer`, one per process) [doc]. [inference] The
  `first frame` mark, the GDI-window hand-off (decision 0075) and the in-game menu's
  screenshot (`PacedMetalLayer.lastPresented`) therefore work unchanged. Measured: the mark
  works; the screenshot finds no frame (KosmicKrisp's drawables are framebuffer-only, for DXVK
  too; chunk 2 evidence).
- `wglSwapIntervalEXT` → `eglSwapInterval` → Zink present mode (FIFO for ≥ 1, IMMEDIATE/MAILBOX
  for 0) [tree: `zink_kopper_set_present_mode_for_interval`]. The frame limit is the game
  layer's pacer, as for DXVK and DXMT (decision 0088; measured in chunk 2).
- Mode changes: Playport does not emulate mode sets; a mode-sized window's swap chain is
  aspect-fitted by the layer, as DXVK's (decision 0088).
- **DXVK and GL together:** separate `VkInstance`/`VkDevice`s on the same KosmicKrisp; fine
  sequentially. A title presenting GL and D3D to the one game layer at the same time is
  unsupported (documented limit). wined3d (GL) under an i386 title set to DXMT becomes a
  real path; decision 0047's routing is unchanged.
- Background: the DXMT gate (dxmt 0002) does not cover KosmicKrisp commits [doc]; Zink
  inherits whatever the Vulkan backend does. Chunk 2: drawing resumes after the background.

### i386 through WoW64 (chunk 3)

- Generator-level conversion in `make_opengl`'s WoW64 output (a `wine-unix` patch, not a
  hand edit of generated files): pointer parameters through the window, `GLintptr`/
  `GLsizeiptr`/handles as values, `get_teb64` through the TEB32 pair.
- **Pointer-or-offset parameters** (≈ 40 entry points: `gl*Pointer*`, `glVertexAttrib*Pointer`,
  `glDrawElements*`, `glDrawRangeElements*`, `glMultiDrawElements*`, `gl*Indirect`,
  `glTex(Sub)Image*`, `glCompressedTex*`, `glReadPixels`, `glGetTexImage`, `glBitmap`,
  `glPolygonStipple`, `glInterleavedArrays`, …): convert only when no buffer is bound to the
  relevant target, asking Mesa for the binding at the call (`GL_ARRAY_BUFFER_BINDING`,
  `GL_ELEMENT_ARRAY_BUFFER_BINDING`, `GL_PIXEL_UNPACK/PACK_BUFFER_BINDING`,
  `GL_DRAW_INDIRECT_BUFFER_BINDING`) [inference: a native state read, cheap]. Host test
  like `build/wow64/vulkan_ptr_test.c`.
- Host→guest pointers: `glGetPointerv`; `wow64_map_buffer`'s "lucky" test
  (`ULongToPtr(PtrToUlong(ptr)) == ptr`) becomes "inside the window", and `buffer_vm_alloc`'s
  `zero_bits` allocates in the window; debug-callback user pointers stay guest values.
- Persistent maps: out of scope at first (refused and logged, 0029); later via Zink placed maps
  in the window.
- **Measurement:** the i386 and x86-64 `gl21` probes' stress mode (20,000 `glVertex3f` per frame
  in `glBegin/glEnd`, and the same as one VBO draw) reports ms per frame; the budget is set in
  the decision record before the run (proposal: stress ≤ 16 ms on i386).

### Size, licensing, notices

- Size: not measured. [inference] Zink-only libgallium plus libEGL for iOS is in the 10–30 MB
  range uncompressed; chunk 1a records the real bytes (`pp verify` output, IPA size delta).
- Licence: Mesa is already shipped (KosmicKrisp, MIT per file, LICENSING.md row). Zink, the
  GL frontend (`src/mesa`, `src/mapi`), `src/egl` and gallium auxiliary bring new files under
  Mesa's per-file licences (mostly MIT; also SGI/Khronos-style notices) [inference from Mesa
  `docs/license.rst`]. The `notices` stage will stop on unclassified files: classify them by a
  reviewed edit of `build/app-notices.json`; update LICENSING.md's KosmicKrisp row to "Mesa
  (KosmicKrisp, Zink, EGL)" and NOTICES.md. No MoltenVK headers may be compiled in (Apache-2.0
  but avoidable; patch 1 removes them).

## Chunks

Each chunk: `./pp test`; committed with its docs and build records; Linear PLA-162 updated
(progress, IPA sha256, run dirs); phone runs follow AGENTS.md (one lock session for
install + plays; Hollow Knight `app-367520 --until first-frame+10 --shot` after any runtime
change; Portal 2 `app-620` after any i386 change). Games played get their Compatibility issue
updated (subagents list them for the supervisor).

**0. "OpenGL games are not supported yet" (if not already done; not found in the docs read).**
Adoption records OpenGL evidence with decision 0046's walker (`opengl32.dll` import or
`wgl*`/`gl*` API imports, guarded dynamic-loading references, as for Direct3D); the catalogue
keeps it; a launch that ends early (or within N s with no first frame) of a title with OpenGL
and no Direct3D evidence says "OpenGL games are not supported yet" instead of "stopped
unexpectedly". [Owner check: decision 0034 removed compatibility labels, so this plan proposes
the message only on a failed launch, not as a page label.] Done: PlayportKit host tests with
fixtures (GoldSrc-style dynamic `opengl32` load, a static import, a D3D9+GL executable); a UI
run of the x86-64 `gl21` probe shows the message. Removed again in chunk 4.

**1a. Mesa GL builds for iOS (host-only).** Mesa patches 1–5; a `mesa.sh` stage for the GL
build and its framework(s); `check-macho-imports.py` clean; the Zink-on-mock host test passes;
notices classified; sizes recorded. Done: `pp build` produces a verified IPA with the new
framework(s) and an unchanged runtime (nothing loads them yet); `pp test` clean.
Phone: Hollow Knight `first-frame+10` (proves the added frameworks do not disturb launch).
*Done 2026-10-11:* `patches/mesa` 0022–0026, `MesaEGL.framework` (≈1 MB) and
`MesaGallium.framework` (≈19 MB; IPA +5.2 MB deflated), linked and embedded like
KosmicKrisp; host test reports Zink on the KosmicKrisp mock at GL 3.3 compatibility;
HK first frame +10.04 s ([evidence](../evidence/2026-10-11-mesa-gl-ios-build.md)).
The notices edit awaits the owner's licensing review.

**1b. Prove the route: glClear and a triangle (x86-64).** The `gl21` probe (Track 3,
`tools/compat/probes/`, llvm-mingw, built into `$PLAYPORT_BUILD/compat/probes/`, imported with
`pp ui --action import:PATH`): `ChoosePixelFormat`/`SetPixelFormat`, `wglCreateContext`,
logs `GL_VERSION`/`GL_RENDERER`/`GL_EXTENSIONS` through `OutputDebugString`, clears to a
colour, draws a fixed-function triangle and a GLSL 1.20 triangle, `SwapBuffers` for N s,
exits 0. Wine side: `SONAME_LIBEGL`, `winios_OpenGLInit`, opengl32 unix tables (x86-64 only;
WoW64 keeps the GL-absent table). Done: on the phone the probe's run has a `first frame` mark,
the screenshot shows the clear colour and both triangles (described in evidence, not
published), the log shows Zink on KosmicKrisp and win32u's core/compat versions; Hollow Knight
`first-frame+10`; Portal 2 to its menu, logs free of EGL/Zink lines. Evidence
`docs/evidence/<date>-opengl-zink-route.md`; decision record "OpenGL through Zink on the host"
(route, ceiling, limits).
*Done 2026-10-11:* `tools/compat/probes/gl21.c` (`build.sh`), `patches/madeira-unix` 0114,
`patches/mesa` 0027 (KosmicKrisp answers `vkEnumerateInstanceLayerProperties`, which Zink needs
with no loader) and 0023/0024 revised (swap chain sized from the layer's `drawableSize`). The
probe: first frame +3.33 s, 60 fps, teal clear and both triangles, `GL_RENDERER zink Vulkan 1.4(Apple
A19 Pro GPU (MESA_KOSMICKRISP))`, core 3.3 / compat 3.3; the surface is made at the first swap,
so the first frame is lost. HK first frame +10.33 s, Portal 2 +6.02 s (its intro at +10), no
EGL/Zink line in either ([evidence](../evidence/2026-10-11-opengl-zink-route.md),
[decision 0087](../decisions/0087-opengl-through-zink-on-the-host.md)).

**2. WGL completeness on x86-64.** Pixel formats (depth/stencil, sRGB, MSAA), pbuffers,
context sharing, `wglSwapIntervalEXT`, resize and mode-change scaling, the dummy-window
pattern, background/foreground, the in-game menu screenshot, a GL frame limit, Mesa cache
behaviour across launches. Free titles: **DDNet (412220)** forced to its OpenGL backend
(it also has Vulkan) and **Teeworlds (380840)** (machine confirmed by adoption on install).
Done: both reach gameplay on the phone with `pp perf` numbers in evidence; HK unchanged.
*Done 2026-10-11 (22e2abc):* `patches/madeira-unix` 0115 (PLA-180: a window's surface at its first
make-current, the game layer handed between windows, decision 0088), `patches/wine-unix` 0028
(PLA-179: no EGL display means no GL), `patches/mesa` 0028 (sRGB window configs),
`tools/compat/probes/wgl.c`. On the phone the probe's read-backs pass for the first frame before a
swap, a dummy that swaps, depth/stencil, sRGB, 4x MSAA, pbuffers, shared contexts, a second window
and back, resize and a mode change; swap interval 0/1 follow the game layer's frame limit (60, 30,
Off = 120 Hz); background and foreground resume; KosmicKrisp's pipeline cache serves Zink across
launches. `GL_CLAMP_TO_BORDER` colours need `MESA_KK_EXPERIMENTAL=custom_border` (PLA-181, off by
default). DDNet and Teeworlds are not on the test account; Baldur's Gate EE stops at a missing
`OpenAL32.dll`; **Neverwinter Nights: Enhanced Edition (GOG) reaches gameplay at 50 fps**. Open: the
in-game menu's screenshot finds no frame on the KosmicKrisp path (GL and DXVK), and a multisampled
context rebound to a new surface crashes in Zink. HK +10.20 s and Portal 2 +6.07 s unchanged, no
EGL/Zink line ([evidence](../evidence/2026-10-11-opengl-wgl-completeness.md)).

**3. i386 through WoW64 + the route gate.** Generator conversion, pointer-or-offset overrides,
window maps, TEB32-derived WoW64 state; host tests; `gl21` i386 probe (normal and stress).
Done: the i386 probe draws on the phone; stress measured on both machines; the decision
record states host-side for i386 or the guest-side fallback (then a separate chunk builds
it); Portal 2 to its menu and Hollow Knight `first-frame+10` unchanged.
*Done 2026-10-11 (be3a61a, aa4549f):*
- `patches/wine-unix` 0029: `make_opengl` writes the WoW64 thunks with each parameter's
  conversion, and `unix_thunks.c` is regenerated from it:
  - pointers and client objects go through the window;
  - values and DC handles stay as they are;
  - pointer-or-offset parameters stay offsets when a buffer is bound to their target (vertex,
    element, indirect, pixel pack and unpack), read with `glGetIntegerv` at the call;
  - `glGetPointerv` returns a pointer through the window and an offset as it is;
  - the window base, `zero_bits` and the debug dispatcher come from the calling thread.
- `patches/madeira-unix` 0116: an i386 child's opengl32 gets that WoW64 table.
- Tests: the host test `build/wow64/opengl_ptr_test.c`, and checks of the generated lines.
- The `gl21` probe: read-back checks of each converted path, a `stress` mode and an i686 build.

On the phone:
- The i386 probe draws (first frame at +3.28 s) and passes every check, as does x86-64.
- **The gate, decision 0089:** immediate-mode stress takes 8.42 and 8.39 ms per frame on i386 and
  8.40 ms on x86-64, frame-paced at 120 Hz. The 20,000 calls themselves take 4.4 ms on i386 and
  3.7 ms on x86-64. **Host-side serves i386**, and no guest-side fallback is built.
- HK first frame +9.91 s and P2 +5.86 s, with no EGL or Zink line.
- Persistent maps on i386 are refused and logged (follow-up).
- Neither optional i386 GL title reached a frame: Quake II (GOG) is not installable, and NWN
  Diamond lacks `binkw32.dll`.

Details are in the [evidence](../evidence/2026-10-11-opengl-i386.md).

**4. GoldSrc and the reported profiles.** After PLA-9 (detection): **Counter-Strike (10)** with
`-gl` (if the test account owns it; otherwise Half-Life 70 under PLA-25), measured; proxies for
Isaac (i386 GL 2.0) and Geometry Dash (x86-64 GL 2.0) from the Track 2 list. The "not supported
yet" message from chunk 0 is removed; routing unchanged (GL needs no backend choice).
Done: CS reaches a map at a measured frame rate; issues updated with **Status:** and
**Best known:**.
*Done 2026-10-11:* `patches/madeira-unix` 0117 (an i386 image with no code and no relocation
directory moves in the WoW64 window: Wine's forwarder-only `usp10.dll` failed against the game's
`Mss32.dll` at 0x10000000, so libcef and chromehtml failed and the game called a null function).
Counter-Strike `-gl +map de_dust2`: first frame +5.19 s, 59.7 FPS over 180 s at the 60 limit, GPU
3.1 ms. Half-Life `-gl +map c1a0`: 60 FPS once loaded, GPU 12–15 ms. `-soft` still fails
(PLA-164). Isaac and Geometry Dash are not on the account. Chunk 0's message was never built.
HK +10.50 s and P2 +6.21 s unchanged
([evidence](../evidence/2026-10-11-opengl-goldsrc.md)).

**5. Docs.** ARCHITECTURE.md (graphics: GL path; "One process" table row), BUILDING.md (the
Mesa GL stage, options, host test), LICENSING.md/NOTICES.md, `docs/plans/finished.md`; the
plan is removed when its outcomes are recorded.

## Risks

| Risk | Effect | Mitigation |
| --- | --- | --- |
| Kopper/EGL Metal patches are new code in Mesa's WSI | presentation bugs, resize races | keep them small; prior art shows the shape; host test; prefer main-thread-free size queries |
| GL ceiling 3.3 (no fp64) | GL 4.x titles refuse to start | none of the reported titles need 4.x; a per-title version override only with evidence |
| Missing line stipple/smooth/wide lines, custom border colours | wrong-looking lines, `GL_CLAMP_TO_BORDER` colours | Zink's emulations; test `KK_EXPERIMENTAL` custom border in chunk 2 |
| i386 pointer/offset ambiguity | corrupted draws or faults in i386 GL games | binding query per ambiguous call; host test; probe covers client arrays and VBOs |
| i386 per-call cost | immediate-mode games slow | measured gate; guest-side i386 fallback |
| Mesa loaded in every launch through GPU enumeration | memory/time cost for D3D titles | check `get_opengl_gpus`; HK/P2 log gates |
| Two swap chains on the one game layer (dummy windows, mixed GL+D3D) | flicker or errors | one presenting drawable per layer, handed over (decision 0088); mixed GL+D3D a documented limit |
| KosmicKrisp disk-cache reuse, `vk_meta` blit black frames (prior art on macOS) | stale pipelines, black frames | cache off for Zink until checked; watch for black frames in chunk 2 |
| Background GPU work refused | lost frames after app switch | chunk 2 check; extend a gate to KosmicKrisp commits if needed |
| Size | IPA grows | measure in 1a |
| Mesa pin moves | patch rebase | Mesa pin moves only with a KosmicKrisp rebase and the usual gates |

## Out of scope

OpenGL ES, Vulkan-on-GL interop (`GL_EXT_memory_object_win32` for D3D↔GL sharing), GL 4.x via
version override, guest-side Zink for x86-64, persistent maps on i386 (chunk 3 follow-up issue).

---

## Appendix: research record

### Summary

Host-side Zink is the recommended route: Wine 11.19's win32u already implements WGL on EGL
surfaceless with framebuffer emulation, so the Wine work is an iOS `OpenGLInit` plus compiling
opengl32's unix side; the hard parts are Mesa patches for Metal presentation on iOS and the
i386 pointer/offset conversion. A measured i386 gate decides whether i386 stays host-side or
falls back to guest-side Zink over the already-working i386 winevulkan.

### Findings

1. **Claim:** win32u loads EGL before the user driver's `OpenGLInit`, defaulting to surfaceless
   FBO-backed window drawables with no present target. **Sources:** `$PLAYPORT_BUILD/run/unix/wine/dlls/win32u/opengl.c`
   (`display_funcs_init`, `egl_init`, `egldrv_surface_create`). **Support:** direct evidence.
   **Confidence:** high.
2. **Claim:** Zink+EGL is allowed by Mesa's meson on Darwin, but Zink needs MoltenVK headers and
   a Vulkan loader on Apple, and kopper lacks Metal surfaces. **Sources:** `$PLAYPORT_BUILD/run/mesa/mesa/meson.build`,
   `src/gallium/drivers/zink/{meson.build,zink_screen.c,zink_kopper.c}`. **Support:** direct.
   **Confidence:** high.
3. **Claim:** KosmicKrisp meets Zink's GL 3.3 list except custom border colour (experimental),
   non-Bresenham line modes; it has no `shaderFloat64`, and GL 4.0 needs fp64, so the ceiling is
   GL 3.3 unless soft-fp64 is enabled. **Sources:** `kk_physical_device.c`, `src/mesa/main/version.c`,
   Zink docs. **Support:** direct (lists) + interpretation (ceiling). **Confidence:** medium.
4. **Claim:** Zink over KosmicKrisp runs GL apps on macOS with an EGL surfaceless Metal-surface
   patch, needing a version override for 4.6. **Sources:** public gist (appendix). **Support:**
   direct, single unreviewed source. **Confidence:** medium.
5. **Claim:** Wine's opengl32 WoW64 thunks use `ULongToPtr` for both pointers and integer
   values; persistent maps need opaque-fd Vulkan export, which KosmicKrisp lacks.
   **Sources:** `unix_thunks.c`, `unix_wgl.c`, `kk_physical_device.c`. **Support:** direct.
   **Confidence:** high.
6. **Claim:** Playport's i386 Vulkan path needed window conversion of ~2,100 pointer uses and
   placed maps, and works (Portal 2). **Sources:** `docs/evidence/2026-10-04-portal2-d3d9-vulkan.md`.
   **Support:** direct. **Confidence:** high.
7. **Claim:** A guest-side Zink `opengl32` exists only as an unmerged WineHQ draft (!10531),
   reported slower than the unix bridge. **Sources:** WineHQ MR page; news write-up.
   **Support:** direct (MR page) + secondary (performance remark). **Confidence:** medium.
8. **Claim:** per-GL-call i386 cost under host-side is significant for immediate-mode games.
   **Support:** researcher inference, unmeasured. **Confidence:** low.

### Contradictions

- Track 6.1 calls host-side "one build serves i386 and x86-64 through Wine's opengl32 thunks";
  the thunks exist, but on Playport's windowed WoW64 they need pointer/offset-aware conversion
  and lose persistent maps, so "serves" needs chunk 3's work and gate.
- Track 6.1 says guest-side "touches less of the driver"; true for presentation (it reuses the
  win32 → Metal Vulkan surface mapping), but it needs Mesa PE builds and Wine-side gdi32/opengl32
  integration that exists only in an unmerged draft.

### Missing evidence

- Mesa GL dylib sizes for iOS; whether gallium can be linked into libEGL as one dylib.
- Whether Zink/st enable soft-fp64 on a device without `shaderFloat64` (GL 3.3 vs 4.x).
- Whether `get_opengl_gpus` runs in every launch on iOS.
- Whether `LIBGL_ALWAYS_SOFTWARE` makes Zink pick a CPU device (`zink_screen.c` not read).
- Whether the adoption-time OpenGL detection (chunk 0) is already implemented (PlayportKit not
  searched; no shell was available in this run).
- `kk_image_max_dimension`/`KK_MAX_VIEWPORTS` values (GL 4.1 limits; irrelevant at 3.3).
- How the Vulkan backend handles iOS background refusal (no KosmicKrisp gate patch in the series).
- Ownership of Counter-Strike (10) on the test account; machine/API of DDNet and Teeworlds builds.
- Per-call transition cost on the phone (both machines).

### Sources

- Kept: pinned Wine 11.19 tree `$PLAYPORT_BUILD/run/unix/wine` (`dlls/win32u/opengl.c`,
  `include/wine/opengl_driver.h`, `dlls/opengl32/unix_wgl.c`, `unix_thunks.c`,
  `dlls/winewayland.drv/opengl.c` as the EGL-driver model) — the interface Playport must implement.
- Kept: pinned Mesa tree `$PLAYPORT_BUILD/run/mesa/mesa` (`meson.build`, `meson.options`, `src/egl/meson.build`,
  `src/egl/drivers/dri2/platform_surfaceless.c`, `include/kopper_interface.h`,
  `src/gallium/drivers/zink/*`, `src/kosmickrisp/vulkan/kk_physical_device.c`, `kk_debug.h`,
  `src/mesa/main/version.c`) — build feasibility and feature level.
- Kept: `patches/madeira-unix` 0029, 0068, 0099; `patches/mesa/series`; `build/stages/mesa.sh`,
  `build/stages/unix.sh`; `app/Sources/WinIOS/IOSDisplayShim.m`; ARCHITECTURE.md, BUILDING.md,
  LICENSING.md, decisions 0003/0046/0047/0075; evidence 2026-10-03 and 2026-10-04 (Portal 2).
- Kept: Zink driver docs, https://docs.mesa3d.org/drivers/zink.html — per-version Vulkan
  requirements.
- Kept (strip from the committed plan if `pp names` objects): macOS Zink-on-KosmicKrisp write-up,
  https://gist.github.com/lucamignatti/5312f5e937de2ba44256ecba6de54cc2 — prior art for Metal
  surfaces in surfaceless and its pitfalls; WineHQ draft MR !10531,
  https://gitlab.winehq.org/wine/wine/-/merge_requests/10531 — guest-side route status; Mesa
  commit e9a87f80 (MR 44048, "zink: do not use implicit sync on kosmickrisp") — upstream already
  tunes Zink for KosmicKrisp.
- Deprioritized: news/forum coverage (Phoronix, GamingOnLinux, krakatech) — secondary; LunarG
  conformance announcement — Vulkan 1.3 conformance on macOS, not iOS or Zink.

### Next steps

1. Chunk 1a's build spike answers sizes, single-dylib linking and the no-DRM probe.
2. Read `zink_screen.c` device selection and fp64 handling; `win32u/sysparams.c` for
   `get_opengl_gpus`; PlayportKit for existing OpenGL evidence (chunk 0).
3. Write the `gl21` probe with its stress mode early: it is the gate for chunks 1b and 3.

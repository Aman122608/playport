# 0093: OpenGL on KosmicKrisp: custom border colours for Zink, fans as triangle lists

**Status:** accepted 2026-10-12, carrying out issue sweep 2.8 (the Zink follow-ups); the choices are
the worker's conservative ones (owner away).
[PLA-181](https://linear.app/playportdev/issue/PLA-181),
[PLA-191](https://linear.app/playportdev/issue/PLA-191). Amends
[0088](0088-opengl-window-surfaces-and-pacing.md) (custom border colours) and
[0087](0087-opengl-through-zink-on-the-host.md) (its "missing Zink requirements").
[Evidence](../evidence/2026-10-12-zink-followups.md).

## Context

- 0088 kept custom border colours off by default. KosmicKrisp exposes `VK_EXT_custom_border_color`
  only behind `MESA_KK_EXPERIMENTAL=custom_border`. That flag is read once per process, so it would
  change every Vulkan title too. With the flag, the `wgl` probe read the right `GL_CLAMP_TO_BORDER`
  colour.
- Half-Life took 12–15 ms of GPU time a frame in c1a0, against 3 ms for Counter-Strike in de_dust2.
  A frame capture showed 136 Metal passes in a Half-Life frame. 68 of them were compute passes, each
  between two render passes that load and store the 640×480 colour and depth targets. Metal has no
  triangle fans, so KosmicKrisp unrolls every fan draw in a compute pass first. GoldSrc draws its
  models with fans.

## Decision

- **A Zink instance gets custom border colours** (`patches/mesa` 0032). KosmicKrisp exposes
  `VK_EXT_custom_border_color` and `VK_EXT_border_color_swizzle` to an instance whose engine is
  `mesa zink`, or when `MESA_KK_EXPERIMENTAL=custom_border` is set, as before. DXVK, vkd3d-proton and
  every other Vulkan instance are unchanged. OpenGL's `GL_CLAMP_TO_BORDER` colours are right by
  default; the per-game option is no longer needed for GL.
- **Zink draws no triangle fans on KosmicKrisp** (`patches/mesa` 0033). Zink reports no fan support
  for KosmicKrisp's driver ID. Mesa's gallium frontend then turns fans, and quads (which Zink would
  otherwise draw through a geometry shader, also compute on KosmicKrisp), into triangle lists on the
  CPU. This is the path Zink already takes on drivers whose portability subset has no fans.

## Consequences

- Half-Life in c1a0 takes 1.59 ms of GPU time a frame (from 15.28 ms), and Counter-Strike in
  de_dust2 1.57 ms (from 3.08 ms). The picture is unchanged (the same c1a0 view, before and after).
  Half-Life's system power falls from 4.66 W to 2.85 W, and its CPU instructions a frame from 124 M to
  101 M.
- The OpenGL probes pass every check, the border check included, on x86-64 and i386; the i386
  immediate-mode stress stays at 8.39 ms a frame (0089's budget is 16 ms).
- Direct3D titles are unchanged: Portal 2 (DXVK) keeps 15.2 ms of GPU time and about 61 M
  instructions a frame. Hollow Knight (DXMT) does not use KosmicKrisp.
- A GL title that draws large fans pays for the conversion on the CPU (an index list per draw)
  instead of in a compute pass. No title is known where that costs more than the render-pass splits
  it replaces.

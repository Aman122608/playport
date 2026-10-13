# 0100: Vulkan is the rendering direction; DXMT is kept only until it can be dropped

**Status:** accepted, 2026-10-12, by the owner ("DXMT paths are ruled out as we are eventually going
to drop it as Vulkan is our choice for the future as it allows vulkan and DX8-12 titles"), recorded
by the speedrun supervisor ([PLA-112](https://linear.app/playportdev/issue/PLA-112),
[PLA-232](https://linear.app/playportdev/issue/PLA-232)). Sets the direction for
[0015](0015-vulkan-backend-accepted.md) and [0044](0044-vulkan-default-for-dx12.md); changes no
default by itself.

## Context

Every IPA carries two Direct3D paths: DXMT (Direct3D 10/11 on Metal) and the Vulkan backend
(KosmicKrisp with DXVK for Direct3D 8–11, vkd3d-proton for 12, and Vulkan titles natively, 0015).
DXMT is the default except for detected Direct3D 12 games (0044). The Vulkan path serves every
title DXMT serves and more (Direct3D 8/9 and 12, Vulkan, and OpenGL through Zink, 0087). Sweep 2.11
found a gap only on Vulkan (videos in a D3D11 shared texture, PLA-121/PLA-232), and one way out
would have been to send the video through DXMT.

## Decision

- **Direction.** Vulkan is the backend Playport develops toward; DXMT will eventually be dropped. A
  later record moves the default and removes DXMT, when Vulkan covers what DXMT does today.
- **No DXMT-only fixes.** A problem that appears only on Vulkan is a Vulkan bug, fixed on the
  Vulkan path; no fix, workaround or design routes work through DXMT.
- **DXMT until then.** DXMT stays built, shipped and the default where 0044 leaves it; a
  compatibility result may name it as the backend that works today. It gets fixes needed to keep
  existing titles working, not new features.

## Consequences

- PLA-232 (shared textures) is done on Vulkan only: an in-process handle map or
  `VK_EXT_external_memory_metal`.
- Compatibility work records the Vulkan result for each title, so the gaps that block moving the
  default are visible.

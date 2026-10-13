# 0102: On Vulkan, an unshared D3D11 texture is shared when its handle is asked for

**Status:** accepted, 2026-10-12, taken unattended by sweep 2.11d2 under the owner's approval for
PLA-232 ("If there's no other way … do it this way") and decision
[0100](0100-vulkan-is-the-direction.md) (Vulkan only, no DXMT route)
([PLA-239](https://linear.app/playportdev/issue/PLA-239),
[PLA-65](https://linear.app/playportdev/issue/PLA-65),
[PLA-121](https://linear.app/playportdev/issue/PLA-121)). Amends
[0101](0101-vulkan-shared-memory-through-metal-heaps.md).

## Context

Unity's video output (WindowsVideoTextureOutput) makes its render textures **unshared**, asks each
one for a shared handle (`IDXGIResource::GetSharedHandle`), and opens that handle with
`OpenSharedResource`. The runs show the opening device is the one made with creation flags 0x20
(BGRA support), not the video device (0x820). Windows returns NULL for an unshared
texture, and so did DXVK. Unity then logs `Got null handle from IDXGIResource::GetSharedHandle`
and drops the frame, so Valheim's intro was black on Vulkan
([evidence](../evidence/2026-10-12-vulkan-shared-texture-metal.md)). DXMT answers the same
request with an in-process handle (`patches/dxmt` 0010). 0101 left this open: the memory of such
a texture is not exportable, and only exportable memory can be opened on another device on
KosmicKrisp.

## Alternatives

- **An in-process handle that names the texture.** This is DXMT's answer. On DXVK it fails for
  the reasons 0101 gives: the other device needs its own image over the same memory, and on
  KosmicKrisp it can import memory only through `VK_EXT_external_memory_metal`.
- **Exportable memory from creation** for every render-target or shader-resource texture, or for
  some sizes or formats. There is no reliable signal at creation: Unity's video textures look like
  any other render texture (format 87, bind flags 0x28, no misc flags; Valheim 3840x1920, Hollow
  Knight 1920x1080). Every such texture would get its own dedicated allocation, and it would be
  shared and so could never be relocated or defragmented. This would add a memory and allocation
  cost to every title for the few textures a video uses. Rejected.
- **Exportable memory on the first request** (chosen). The cost is paid only by a texture whose
  handle something asks for.
- **New storage with undefined contents** instead of a copy. Unity writes the texture before each
  use, so this would probably be enough. But DXVK already has a relocation path that copies (the
  one that adds usage to an image), and with it the texture keeps what it held. Rejected as the
  less safe choice.

## Decision

- **DXVK (`patches/dxvk` 0005).** On the first `GetSharedHandle` for a texture made unshared,
  D3D11 shares it if it is a single-sampled 2D texture with `D3D11_USAGE_DEFAULT`, no CPU access
  and no mapping, and is not from D3D11on12:
  - The device injects a high-priority CS chunk and waits for it, as `LockImage` does. The chunk
    calls `DxvkContext::exportImage`.
  - `exportImage` gives the image the sharing a texture made shared gets: export as a D3DKMT
    global (KMT) handle, `shared` (it returns to its default layout at each submission), and no
    further relocation. It allocates dedicated exportable storage in the GENERAL layout, and
    copies the old contents in through `relocateResources`.
  - The chunk then submits at once, so the copy reaches the GPU before another device can write
    to the memory.
  - The texture's runtime description, which the opening device reads, gets
    `D3D11_RESOURCE_MISC_SHARED`, so that device imports the memory. `GetDesc` still reports the
    texture's own flags.
  - The handle returned is the memory's D3DKMT global handle, which 0101's table in
    `patches/wine-unix` 0034 maps to the Metal heap.
- A texture or device that cannot be shared this way still gets NULL, with one warning, and is
  not tried again.
- `CreateSharedHandle` (NT handles) is unchanged: Windows fails it for a texture made without
  `D3D11_RESOURCE_MISC_SHARED_NTHANDLE`, and Unity does not call it.
- **Synchronisation:** none is added. Keyed mutexes and fences are still not shared (0001's
  guard stays). Unity opens these textures without a keyed mutex, as on DXMT, where the video
  works without one.

## Consequences

- **Valheim's intro plays on Vulkan.** Unity's seven requests each get a handle, and all seven
  are opened. Hollow Knight's New Game prologue cinematic plays on Vulkan too: its 16
  video textures are 1920x1080 ([evidence](../evidence/2026-10-12-vulkan-unity-video-unshared-handle.md)).
- **First use of the import path.** These runs are the first time 0034's import path ran in a
  title. KosmicKrisp retains the imported heap and adds it to the importing device's residency
  set, so an opened texture outlives the exporting memory if it has to.
- **Memory.** Only the textures asked for move, and each moves once to a dedicated allocation of
  its own size. The old storage is freed once the copy is done. Nothing changes per frame:
  `GetSharedHandle` on a texture already shared takes a lock and reads its handle.
- **Behaviour change for every title on Vulkan.** Any title that asks an unshared texture for a
  shared handle now gets one where Windows gives NULL. That texture then stays put in memory: it
  cannot be relocated, so DXVK can no longer move it to add usage. DXMT has made the same trade
  since its 0010.
- **No synchronisation between the devices.** If one device writes while the other reads, a
  frame could tear. No run has shown it.

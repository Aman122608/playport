# 0101: A shared D3D texture on Vulkan shares its memory as a Metal heap, within the process

**Status:** accepted, 2026-10-12, taken unattended by sweep 2.11d under the owner's approval
("If there's no other way for PLA-232 then let's do it this way", for
`VK_EXT_external_memory_metal`) and decision [0100](0100-vulkan-is-the-direction.md) (Vulkan only,
no DXMT route) ([PLA-232](https://linear.app/playportdev/issue/PLA-232),
[PLA-121](https://linear.app/playportdev/issue/PLA-121)). Extends
[0061](0061-dxvk-as-a-series.md): `patches/dxvk` 0001 stays.

## Context

A D3D11 texture made with `D3D11_RESOURCE_MISC_SHARED` gets a handle (D3DKMT global, or NT) that
another D3D11 device opens over the same memory. DXVK makes one through
`VK_KHR_external_memory_win32`, and Wine's win32u builds that extension on the host's
`VK_KHR_external_memory_fd`: the memory's fd travels in a D3DKMT resource object. KosmicKrisp has
no fd export. It has `VK_EXT_external_memory_metal`: every `VkDeviceMemory` is a placement
`MTLHeap`, which it exports and imports (retaining it). So on iOS no Vulkan device had the Win32
extension, and DXVK made such a texture without a handle (0061's 0001). Separately, Wine on iOS lists no display GPU,
so no Vulkan device has a LUID, and DXVK made no D3DKMT adapter or device. Every shared-resource
call needs that device.

## Alternatives

- **An in-process handle map that names the producing image.** Every device of a title is in one
  process, but each D3D11 device is its own `DxvkDevice`, `VkDevice` and Metal queue with its own
  residency set. A `DxvkImage` or `VkImage` from one cannot be used on another: its views,
  barriers and lifetime tracking belong to its device. On KosmicKrisp the second device would also
  sample memory missing from its residency set. Naming the image is not enough: the second device
  has to import the memory, and on KosmicKrisp only `VK_EXT_external_memory_metal` imports memory.
  Rejected as a mechanism, kept as the naming: see below.
- **DXVK's own paths.** The legacy "Proton" shared-resource path (`\??\SharedGpuResource`,
  `dxvk_shared_handle`) needs a host kernel device that Wine on iOS does not have. Its open fails
  in every log (`file-wfail … SharedGpuResource`). `D3D11On12` wraps D3D12 resources on one
  device and shares nothing between devices.
- **Wine's D3DKMT shared-resource objects.** These are the handle map: process-wide global (KMT)
  or NT handles with the runtime description DXVK stores. They carry the memory only as an fd.
  They are used, with the memory carried beside them (below).
- **Copying through the CPU** on each open or flush. This is a per-frame copy, with no point at
  which the copy is known to be complete. Rejected.
- **DXMT.** Excluded by 0100.

## Decision

- **win32u (`patches/wine-unix` 0034).** When the driver maps `VK_KHR_external_memory_win32` to
  `VK_EXT_external_memory_metal`, the memory's host handle type is `MTLHEAP`. An export makes its
  D3DKMT resource without an fd and records the heap in a process-local table, keyed by the
  resource's global D3DKMT handle. An NT-shared resource has one too, and every open of it
  returns the same. An import looks the heap up and imports it under the table's lock. The entry
  goes when the exporting memory is freed; the importer's driver keeps its own reference.
  Sharing with another process is not possible this way, and nothing on iOS needs it.
- **winios driver (`patches/madeira-unix` 0131).** Maps the two extensions both ways, so devices
  report `VK_KHR_external_memory_win32` and enable the Metal one on the host.
- **DXVK (`patches/dxvk` 0003).** A device with `VK_KHR_external_memory_win32` and no LUID opens
  its D3DKMT adapter with a zero LUID. Wine opens an adapter for any LUID, and DXVK uses the
  adapter only to make the D3DKMT device that shared resources need. A device with a LUID is
  unchanged.
- **DXVK (`patches/dxvk` 0004, diagnostics).** Logs each D3D11 device's creation flags, the first
  32 shared textures made or opened, handle requests (also for a texture made unshared) and opens.
  It changes no behaviour.
- Semaphores and fences are not shared: no `VK_KHR_external_semaphore_win32` mapping, and 0001's
  guard stays.

## Consequences

- On Vulkan a shared D3D11 texture now gets a real handle that another device in the title can
  open. Valheim's six Media Foundation output textures are made shared, with no error.
- **Valheim's intro is still black** ([evidence](../evidence/2026-10-12-vulkan-shared-texture-metal.md)).
  Unity never asks for those textures' handles. Each frame it asks for the handle of its own
  render texture, which it made **unshared** (`D3D11: shared handle 0 for a unshared 3840x1920
  texture`). It then gives up on the NULL that D3D11 returns. DXMT answers that request with an
  in-process handle (`patches/dxmt` 0010), which is why the video shows there. Doing the same in
  DXVK means making an unshared texture's memory exportable on request: its storage would move to
  a dedicated exportable allocation. That needs its own decision (follow-up to PLA-232).
- Every Vulkan device on iOS now enables `VK_EXT_external_memory_metal` and reports Win32 external
  memory. DXVK's shared-resource tier and `D3D11_FORMAT_SUPPORT2_SHAREABLE` answers follow it.
  Hollow Knight (Direct3D 11) and Portal 2 (Direct3D 9, i386) reach their first frames on Vulkan
  with it.
- The import path ran in no title on the phone: no title opened a handle.

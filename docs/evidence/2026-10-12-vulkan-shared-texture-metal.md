# Evidence: shared D3D11 textures on Vulkan through Metal heaps; Valheim's intro still black (PLA-232)

**Date:** 2026-10-12. **Kind:** phone runs (dev app), sweep 2.11d. **Linear:**
[PLA-232](https://linear.app/playportdev/issue/PLA-232),
[PLA-121](https://linear.app/playportdev/issue/PLA-121). **Decision:**
[0101](../decisions/0101-vulkan-shared-memory-through-metal-heaps.md). The owner was away.
Earlier record: [vulkan-video-shared-texture](2026-10-12-vulkan-video-shared-texture.md).

**Final IPA:** `Playport-26.5-6f430560.ipa` (dev), sha256
`6f43056098494d557e01469482247c36e5c38b5edc1623a57eb26ec78482beb6`. It has `patches/wine-unix` 0034,
`patches/madeira-unix` 0131 and `patches/dxvk` 0003 and 0004.

## Runs

Each Valheim run (Steam 892970) set Vulkan for its session only
(`--settings 'app-892970:{"graphics":"vulkan"}'`). Run directories are under
`$PLAYPORT_BUILD/ui-runs/`.

| Run | IPA (sha256 prefix) | Change | Result |
| --- | --- | --- | --- |
| `s11d-valheim-vk` | `a4a16eea` | wine-unix 0034, madeira-unix 0131 | `Failed to create shared resource: VK_KHR_EXTERNAL_MEMORY_WIN32 not supported` is gone. Six `DxvkMemoryAllocator::createImageResource: Failed to open shared D3DKMT handle` and `D3D11: Failed to write shared resource info for a texture`. Screen black at 30, 42 and 50 s |
| `s11d-valheim-vk2` | `f5885360` | + dxvk 0003 | no shared-resource warning at all. Screen black (every pixel 0) at 30, 42 and 50 s and at the stop |
| `s11d-valheim-vk3` | `9378601d` | + dxvk 0004, first form (shared handles and opens only) | no handle asked for, no open |
| `s11d-valheim-vk4` | `797e38b8` | 0004 also logs devices, shared textures, and cross-device views and copies | below. No cross-device view or copy |
| `s11d-final-valheim-vk` | `6f430560` | 0004 final form (also logs requests for an unshared texture's handle) | below. Screen black at 45 s and at the stop (+48 s) |
| `s11d-final-hk-vk` | `6f430560` | Hollow Knight (367520) on Vulkan, `--pad` | first frame +10.5 s. Main menu, then Start Game and the cursor's slot by pad: the slot held a save, which loaded (the Knight on a bench, HUD drawn, 6 and 16 s later). The New Game prologue was **not** reached |
| `s11d-final-p2` | `6f430560` | Portal 2 (620), its default (Vulkan, Direct3D 9, i386) | first frame +5.9 s. "powered by Source" at +10 s |

## What the logs show (final IPA, Valheim)

- Two D3D11 devices: `D3D11Device: device …c38, creation flags 20` (BGRA support: Unity's
  renderer) and `… flags 820` (video support plus BGRA: Unity's video device, given to Media
  Foundation).
- The video device makes six shared textures: `makes a shared 3840x1920 texture, format 87, bind
  flags 8, misc flags 2`. These are the source reader's output samples
  (`MF_SA_D3D11_SHARED_WITHOUT_MUTEX`). They now get exportable memory with no error. No device asks for their handles,
  and nothing opens one (no `OpenSharedResource*` call in any run).
- Once the video starts, Unity asks again and again for a handle to its own **unshared** render texture.
  The first 32 are logged: `D3D11: shared handle 0 for a unshared 3840x1920 texture, format 87,
  misc flags 0`. D3D11 returns NULL for such a texture, and Unity gives up on that frame.
  `patches/dxmt` 0010 describes the same request on DXMT ("Got null handle from
  IDXGIResource::GetSharedHandle"). There DXMT answers it with an in-process handle, which is why
  the intro shows on DXMT.

The earlier record's reading, that Unity opens the shared Media Foundation texture through
`GetSharedHandle`, does not hold. The handle that matters is the unshared render texture's.
Making that texture shareable on request in DXVK is not done here (decision 0101, Consequences).

## Root causes found on the way

- Wine on iOS lists no display GPU: `lock_display_devices` takes the service-process shortcut,
  with a virtual monitor. So win32u finds no LUID for the Vulkan device, and DXVK's DXGI prints
  its fallback `Adapter LUID 0: 0:3ec`. DXVK then had no D3DKMT adapter, so
  `D3DKMTOpenResource2` failed for every exported texture (run 1). `patches/dxvk` 0003 opens the
  adapter with a zero LUID when the device can share memory.

## Not checked

- The import path, a second device opening a handle, never ran: no title opened one.
- Hollow Knight's New Game video on Vulkan (the 0061 regression case): the slot the pad picked
  held a save.
- Performance: not measured. No per-frame path changed: allocations are exportable only for a
  texture made shared, and 0004 logs only shared-resource events.

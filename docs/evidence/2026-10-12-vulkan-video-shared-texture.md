# Evidence: a Unity intro video is invisible on Vulkan because DXVK cannot share a texture (PLA-121)

**Date:** 2026-10-12. **Kind:** one phone run (dev app). **Linear:**
[PLA-121](https://linear.app/playportdev/issue/PLA-121) (videos invisible on Vulkan), title
Valheim (Steam 892970). The owner was away; reproduction first, as the sweep asked, and no fix
within its half hour. **IPA:** `Playport-26.5-12062a00.ipa` (dev, sha256
`12062a00f1624405dc4fa8e529eab303ed4b29ef6036e56bcafb4428702c8bbd`). Run directory:
`$PLAYPORT_BUILD/ui-runs/s11-valheim-vk`.

## Run

Valheim (installed; its page is on DXMT) played on Vulkan for the session only (`--settings
'app-892970:{"graphics":"vulkan"}'`, not kept), to first frame +40 s with a screenshot at 25 s:

- First frame at +7.2 s. At 25 s the screenshot shows the Valheim logo on its dark background, the
  splash before the intro cinematic. At the stop (+47 s) the screen is all black: the cinematic is
  playing and nothing of it shows. On DXMT the same cinematic shows
  ([steam-ticket-valheim](2026-10-07-steam-ticket-valheim.md)).
- DXVK's log, six times each: `Failed to create shared resource: VK_KHR_EXTERNAL_MEMORY_WIN32 not
  supported` and `D3D11: Failed to write shared resource info for a texture`, after one
  `D3D11DeviceFeatures: External memory features not supported`.

So the 2026-10-07 diagnosis still holds on today's build. Unity's video player renders its frames
into a D3D11 texture created shared and opens it through `IDXGIResource::GetSharedHandle`. DXVK
makes a shared texture only through `VK_KHR_external_memory_win32`, which KosmicKrisp does not
have. `patches/dxvk` 0001 (decision 0061) keeps the game alive without the handle, but the frames
never reach the picture. Under Proton the driver has the extension, so the same game shows its
video there. Monster Train's video playing now (owner's report on the issue) does not contradict
this: a title that decodes into an unshared texture is not affected.

## Next step (not built)

KosmicKrisp exposes `VK_EXT_external_memory_metal` (`kk_physical_device.c`), and every D3D11
device of a title is in the same process. A `patches/dxvk` change could export a shared texture's
memory as its Metal handle, hand the guest a process-local handle for it, and import that handle
on the device that opens it. It needs a decision (it is a Playport-only shared-resource path in
DXVK) and a phone check with Valheim's intro on Vulkan.

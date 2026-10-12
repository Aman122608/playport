# Evidence: Unity video on Vulkan, an unshared texture's handle on request (PLA-239)

**Date:** 2026-10-12. **Kind:** phone runs (dev app), sweep 2.11d2. **Linear:**
[PLA-239](https://linear.app/playportdev/issue/PLA-239),
[PLA-65](https://linear.app/playportdev/issue/PLA-65),
[PLA-121](https://linear.app/playportdev/issue/PLA-121). **Decision:**
[0102](../decisions/0102-unshared-texture-handle-on-request.md). The owner was away. Earlier
record: [vulkan-shared-texture-metal](2026-10-12-vulkan-shared-texture-metal.md).

**IPA:** `Playport-26.5-e99cb8e7.ipa` (dev), sha256
`e99cb8e719e123f44ec6dacd8d77c7ccd94f054c7ada8fd7fc1b161fd86f42dd`. It is the earlier IPA
(`6f430560`) plus `patches/dxvk` 0005. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.
The committed 0005 differs from the one this IPA was built with only in a source comment and the
commit message (the opening device is named correctly). Its rebuild, `Playport-26.5-92e4690f.ipa`
(sha256 `92e4690f24147c50102e8cd15db3769af6f7984be14c07ce450e298191a51103`, not installed), has
the same `app/artifacts.tsv`: every staged runtime file has the same sha256.

## Runs

| Run | Title | Result |
| --- | --- | --- |
| `s11d2-valheim-vk` | Valheim (892970), Vulkan for the session (`--settings 'app-892970:{"graphics":"vulkan"}'`), `--until first-frame+50` | First frame at +7.2 s. **The intro cinematic plays.** Screenshots at 38, 45 and 53 s after launch (about first frame +27, +34 and +42 s) show its scenes: a cloaked figure on a throne among roots, a battlefield seen from a ridge, a skeletal beast with the subtitle "deathless creatures older than any others." |
| `s11d2-hk-vk-newgame` | Hollow Knight (367520), Vulkan for the session, `--pad`, then `pp pad send` and `pp phone shot` | First frame at +10.3 s. Start Game; the profile screen (`profiles.png`) had slots 1, 2 and 4 saved and slot 3 reading New Game, cursor on slot 1; DOWN, DOWN, A started a new game in slot 3. The prologue poem shows (`prologue-a`, `-b`), then **the opening cinematic plays** (`prologue-d`: glowing eyes; `prologue-f`: the Knight in a misty landscape, pillarboxed video). The app was ended in the cinematic, so slot 3 was not saved |
| `s11d2-final-hk` | Hollow Knight, its default (Vulkan), `--until first-frame+10 --shot` | First frame at +8.7 s; the main menu at the stop |
| `s11d2-final-p2` | Portal 2 (620), its default (Vulkan, Direct3D 9, i386), `--until first-frame+10 --shot` | First frame at +5.8 s; "powered by Source" at the stop |

## What the logs show

Valheim (`s11d2-valheim-vk/pull/s1-host.log`):

- Unity asks for the handle of each of seven unshared render textures once. Each request now
  succeeds: `D3D11: shares a 3840x1920 texture made unshared, format 87, as 40000182`, and so on
  through `40000302`, followed by `D3D11: shared handle 40000182 for a unshared 3840x1920 texture`.
- Each handle is opened, by the device made with creation flags 0x20 (BGRA support:
  `D3D11Device: device …c38, creation flags 20`), not by the video device (flags 0x820):
  `D3D11Device::OpenSharedResource: device …c38 opens 0x40000182`. That device then reports
  `D3D11: device …c38 opens a shared 3840x1920 texture, format 87, bind flags 28, misc flags 2`.
  The runtime description now says shared, so the device imports the memory. The log does not say
  which device made the unshared textures, because the request line names no device. If it was
  the video device, the frames cross devices; if it was the same 0x20 device, the open imports
  memory back onto the device that made it. Either way the open worked and the video shows.
- These are the first imports through `patches/wine-unix` 0034 in a title. The log has no
  `No Metal heap`, no `Cannot share` and no `Failed to` line from DXVK or win32u. Its
  `err:`/`warn:` lines are DXGI's monitor and EDID lines, which were in earlier runs too.
- The six shared Media Foundation output textures are made as before (`makes a shared 3840x1920
  texture … misc flags 2`), and no device asks for their handles.

Hollow Knight (`s11d2-hk-vk-newgame/pull-after/s1-host.log`, `graphics: vulkan`):

- 16 requests, each answered: `D3D11: shares a 1920x1080 texture made unshared, format 87`.
- 16 opens (`OpenSharedResource … opens 0x40000182` and on), again by the device made with
  flags 0x20 (`…638`; the video device is `…108`, flags 0x820), with no sharing error.

## Design notes checked

- Unity asks for the handle once per texture, not once a frame, now that it gets one: seven
  requests for Valheim and 16 for Hollow Knight. Each texture moves its storage once.
- Unity uses no keyed mutex on these textures (misc flags 0), and the video shows without one.

## Not checked

- Performance: not measured. No per-frame path changed: a texture moves once, when its handle is
  first asked for, and a later request takes a lock and reads the handle.
- Whether the two unsynchronised devices ever tear a frame: only still screenshots were taken.
- Valheim past its intro on Vulkan: not played.

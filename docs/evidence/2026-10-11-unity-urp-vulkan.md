# Unity 6 URP scenes on Vulkan: KosmicKrisp's workaround 6 on iOS 27 (PLA-161)

**Date:** 2026-10-11. **Phone:** iPhone18,4 (A19 Pro), iOS 27.0, dev IPAs upgraded in place.
**Title:** Cat Mail Co. Demo (Steam 4622530): Unity 6 IL2CPP, x86-64, Direct3D 11 (FL 11_1), URP
with the GPU Resident Drawer's indirect draws. Its Vulkan default comes from `UnityPlayer.dll`
importing `d3d12`.

## Symptom

On Vulkan (DXVK 3.1.1 over KosmicKrisp) the first-person tutorial's warehouse drew as a blown-out
white void over a flat floor: most objects missing, a few (shelves, floor tiles) drawn rotated and
in other objects' places, and a different set from play to play. Menus and 2D UI were right, DXVK
logged no error, and DXMT drew the room correctly (PLA-160's report: `.work/ui-runs/20261011T070622`
on Vulkan, `…T071145` on DXMT).

## Cause

KosmicKrisp turns off its workarounds 1-6, 8, 11, 12, 17 and 18 when
`isOperatingSystemAtLeastVersion(27.0)`, because macOS 27 fixed the Metal bugs behind them. On
iOS the same check is true for iOS 27.0. Workaround 6 (every device memory access `coherent`,
for Metal not keeping its own device-memory coherency model) is still needed on this phone.

- **Capture:** a Vulkan GPU capture of a tutorial frame (`pp gpu capture --frame 7200`,
  `.work/gpu-runs/pla161-vk-1`, 3.4 GB) has 15 command buffers, 77 passes and 1,075 draws. The
  depth prepass and the opaque pass are indirect draws, each render pass behind compute work;
  compute kernels in the frame append instances with `atomic_fetch_add` on texel buffers. This
  pointed at GPU-written draw data, not at a format, sRGB, bloom or MSAA difference.
- **Not the unroll:** `MESA_KK_DISABLE_WORKAROUNDS=13` (no index unrolling for the indirect
  draws) left the scene as wrong (`…T201720`).
- **The bisect** (`patches/mesa` 0029, `MESA_KK_DEBUG=os26_workarounds`, keeps every workaround
  as on macOS 26; each play reached the tutorial through the pad, same view):

| Graphics options | Tutorial | Run |
| --- | --- | --- |
| none (HEAD f5ceeee's runtime) | wrong | `…T200354` |
| `os26_workarounds` | right, as on DXMT | `…T203307` |
| `os26_workarounds`, disable 3, 6, 18 | wrong | `…T203634` |
| `os26_workarounds`, disable 3 | right | `…T203946` |
| `os26_workarounds`, disable 6 | wrong | `…T205448` |

## Change

`patches/mesa` 0030 keeps workaround 6 on iOS for every shader stage but fragment shaders
(unless `MESA_KK_DISABLE_WORKAROUNDS` names it); the pipeline cache key includes the choice.
`app/PlayportKit/Titles/compat-defaults.json` loses Cat Mail Co. Demo's `graphics: dxmt`.

- **Every stage** (first try): the scene was right (`…T210338`), but Portal 2's GPU time a frame
  more than doubled and it fell to 27-32 FPS.
- **Compute shaders only:** the scene was wrong again (`…T212218`).
- **Every stage but fragment:** the scene is right (`…T213307`) and Portal 2 holds 60 FPS.

Portal 2 (`pp perf --title app-620 --secs 120 --pad first-frame+30:p2-cold-boot --pad
first-frame+70:p2-walk`, window 75-120 s, The Cold Boot's first chamber; 720p, 60 FPS limit):

| Workaround 6 | Runs | FPS | GPU ms a frame | GPU busy |
| --- | --- | --- | --- | --- |
| off (`MESA_KK_DISABLE_WORKAROUNDS=6`, the old behaviour) | `pla161-p2-ctl-1` | 59.2 | 11.7 | 76 % |
| every stage | `pla161-p2-wa6-1`, `-wa6-2` | 32.2, 26.8 | 30.0, 37.1 | 100 % |
| all but fragment (0030) | `pla161-p2-vs-1`, `-vs-2` | 59.9, 60.0 | 15.2, 15.2 | 95 % |

The control ran after the hot every-stage runs; its 11.7 ms and the change's 15.2 ms are one pair
each, warm, on the charger. 15.2 ms is within the 13-15 ms the same chamber measured on
2026-10-04 ([portal2-gameplay](2026-10-04-portal2-gameplay.md)), but the GPU headroom at 720p/60 is
smaller: a GPU-bound Vulkan title may lose frames.

## On the committed IPA

Recorded below after the gates.

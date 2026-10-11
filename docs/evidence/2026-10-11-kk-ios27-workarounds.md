# KosmicKrisp's OS 27 workarounds on iOS 27: workaround 6 narrowed to descriptor loads (PLA-193)

**Date:** 2026-10-11. **Phone:** iPhone18,4 (A19 Pro), iOS 27.0, on the charger, dev IPAs
upgraded in place. Follows [unity-urp-vulkan](2026-10-11-unity-urp-vulkan.md) (PLA-161), whose
`patches/mesa` 0030 kept workaround 6 (every device memory access `coherent device`) on iOS for
every shader stage but fragment shaders.

**Supervisor decision (2026-10-12).** The narrowed patch (58534bb) is reverted: the broad 0030
(every device memory access `coherent device` in every stage but fragment shaders, as gated on
246365a) ships, because the narrowing gained nothing measurable (Cost below) and covers fewer
accesses. The narrowed design stays in history (58534bb) for PLA-195. The "Change" section below
and the runs on `7bc21c14…` describe the narrowed patch, not what ships.

## What workaround 6 changes

In `nir_to_msl.c`, `global_access_qualifier()` makes three kinds of access `coherent device`
when workaround 6 is on: `load_global` and `store_global` (storage buffers and buffer device
addresses) and `load_global_constant_offset`. KosmicKrisp emits the last for descriptor set loads
(`kk_nir_lower_descriptors.c`, marked `ACCESS_CAN_SPECULATE`) and for uniform buffers without
robustness. Robust uniform buffer loads (`load_global_constant_bounded`), root-table and
vertex-buffer fetches use the `constant` address space and atomics are always plain `device`, so
workaround 6 does not touch them. Image accesses are unaffected (a read-write image is always
`memory_coherence_device`).

## Which accesses Cat Mail Co. Demo needs coherent

A throw-away patch on top of 0030 (not committed) selected parts of workaround 6 with
`MESA_KK_DEBUG` flags, in the game's Graphics options. Cat Mail Co. Demo (Steam 4622530), Vulkan,
16:9, through the pad to the tutorial warehouse (PLA-161's route), same view:

| Workaround 6 kept (in every stage but fragment unless named) | Tutorial | Run |
| --- | --- | --- |
| everything (0030) | right | `…T223045` |
| not on descriptor or uniform loads (`load_global_constant_offset`; first experiment IPA `1ca2ead3…`) | wrong | `…T222725` |
| not on descriptor loads | wrong (void, a counter drawn huge in the wrong place) | `…T223817` |
| not on uniform buffer loads | right | `…T224121` |
| not on buffer loads and stores | right | `…T224430` |
| vertex shaders only, every access | right | `…T224742` |
| only shaders that write memory | wrong | `…T225046` |
| compute shaders only (PLA-161) | wrong | `…T212218` |
| descriptor loads only (both perf runs below) | right | `pla193-cat-desc-1`, `-2` |

So the scene needs coherent **descriptor set loads in its vertex shaders**: without them a vertex
shader reads stale descriptors and fetches other buffers' data, which matches what the broken
scene showed (objects missing, others rotated and in other objects' places). Coherent buffer and
uniform loads are not needed for it. Descriptor set memory is written by the CPU
(`vkUpdateDescriptorSets` and templates), so the likely underlying fault is a stale GPU cache line
for CPU-written descriptor memory between submissions on iOS 27; that was not proven here
(follow-up below).

## Change

`patches/mesa` 0030 now keeps workaround 6 on iOS 27 for descriptor set loads only
(`load_global_constant_offset` with `ACCESS_CAN_SPECULATE`), in every stage but fragment shaders,
unless `MESA_KK_DISABLE_WORKAROUNDS` names 6; the pipeline cache key includes the choice.
`MESA_KK_DEBUG=os26_workarounds` (0029) still turns every OS 27 workaround back on in every stage,
as [GPU-DEBUGGING](../GPU-DEBUGGING.md) describes. The stage scope stays every stage but fragment
rather than vertex only (also right for the tutorial): compute and other stages read descriptors
the same way, and keeping them cost nothing measurable.

## Cost

Portal 2 (`pp perf --title app-620 --secs 120 --pad first-frame+30:p2-cold-boot --pad
first-frame+70:p2-walk`, window 75-120 s, The Cold Boot's first chamber, 720p, 60 FPS limit), on
the experiment's IPA `6e93509e…`, warm, on the charger:

| Workaround 6 | Runs | FPS | GPU ms a frame | GPU busy |
| --- | --- | --- | --- | --- |
| off (`MESA_KK_DISABLE_WORKAROUNDS=6`) | `pla193-ctl-1`, `-ctl-2` | 60.0, 60.0 | 15.19, 15.20 | 95 %, 95 % |
| every access, all but fragment (old 0030) | `pla193-cur-1`, `-cur-2` | 60.0, 60.0 | 15.14, 15.18 | 95 %, 95 % |
| descriptor loads, all but fragment (new 0030) | `pla193-desc-1`, `-desc-2` | 60.0, 60.0 | 15.19, 13.56 | 95 %, 87 % |
| descriptor loads, every stage | `pla193-descfs-1` | 29.3 | 33.91 | 100 % |

The control did not reproduce PLA-161's 11.7 ms: today workaround 6 off and the old 0030 measured
the same 15.2 ms, so PLA-161's one-pair difference was run-to-run variance, not 0030's cost.
Workaround 6 outside fragment shaders costs Portal 2 nothing measurable either way. Its fragment
cost (PLA-161: 27-32 FPS with it on everywhere) is in the descriptor loads alone: coherent
descriptor loads in fragment shaders halve the frame rate.

Cat Mail Co. Demo, standing in the tutorial warehouse (`pp perf --title app-4622530 --secs 180
--pad first-frame+15:<the tutorial route>`, window 140-180 s, Metal HUD on), runs in the order
listed:

| Workaround 6 | Run | FPS | GPU ms a frame | GPU busy |
| --- | --- | --- | --- | --- |
| every access (old 0030) | `pla193-cat-cur-1` | 45.9 | 19.69 | 94 % |
| descriptor loads (new 0030) | `pla193-cat-desc-1` | 43.7 | 20.72 | 95 % |
| every access (old 0030) | `pla193-cat-cur-2` | 44.4 | 20.39 | 95 % |
| descriptor loads (new 0030) | `pla193-cat-desc-2` | 43.1 | 21.08 | 95 % |

All four drew the warehouse right. The narrowing gained nothing here; the narrowed runs were 1-2
FPS lower, within the spread of alternating hot runs but not shown to be noise.

## The other workarounds OS 27 turns off

KosmicKrisp turns off workarounds 1-6, 8, 11, 12, 17 and 18 at OS version 27 (the mask also
covers the unused bit 0). From `docs/drivers/kosmickrisp/workarounds.rst` in the pinned Mesa.
The A19 Pro is taken to report GPU family Apple10, as the M5 does (Apple's feature tables; not
read from a log), so workaround 2 is on for it regardless (KosmicKrisp re-enables 2 for family
10 and up) and 18's M3-and-later condition holds. A/B on the committed IPA, `MESA_KK_DEBUG=
os26_workarounds` against none, Vulkan: Hollow Knight's title menu (`…T235307` / `…T235535`),
Portal 2's Source splash (`…T234531` / `…T234705`, identical pixels) and Death's Door's title
screen (Epic, `…T235001` / `…T235111`, the same animated menu) looked the same. House of Golf 2 is
still black at first frame+40 either way (`…T235632` / `…T235755`): its title screen comes after
about 240 s ([prerequisites-from-play](2026-10-11-prerequisites-from-play.md)), so it tells
nothing here.

| # | Metal bug (upstream) | Plausibly on A19 Pro, iOS 27 | Symptom in a title we have |
| --- | --- | --- | --- |
| 1 | an uninitialized scratch array crashes the MSL compiler; Apple: fixed in macOS 26 | unlikely (fixed a release earlier) | none: no failed pipeline compiles seen |
| 2 | a loop whose break never changes crashes or hangs the compiler | n/a: on for family 10 anyway | none |
| 3 | `simd_ballot`, quad/simd any/all inside a conditional return every active lane (FB20113490) | possible: compiler bug; would hit subgroup-using shaders (vkd3d-proton, some DXVK paths) | none: Cat Mail right with 3 off under `os26_workarounds` (PLA-161); the A/B above |
| 4 | `simd_is_helper_thread()` stays true after `discard_fragment()` (FB21124215), M1/M2 only | no: hardware-specific | none |
| 5 | an always-taken discard drops a fragment shader's earlier side effects (UAV writes) | possible: compiler | none seen; would show as missing pixel-shader UAV writes |
| 6 | device memory not coherent per the MSL memory model | **yes**: descriptor loads (above) | Cat Mail Co. Demo (fixed by 0030) |
| 8 | GPU capture of small heaps fails under Rosetta 2 (FB22579201) | no: macOS/Rosetta only | none |
| 11 | concurrent `MTL4Compiler` instances corrupt the heap (FB22683138); fixed macOS 27 beta 5 | unlikely: same Metal release; one device a process here | none: no compile-time crashes seen |
| 12 | Metal 4 draws miscompute the index buffer length register: out-of-bounds index robustness | possible: driver; only with out-of-range indices | none |
| 17 | `tanh` with NaN folded wrongly under float controls 2 | unlikely to matter for games | none |
| 18 | stale reads after a device-scope barrier on M3 and later (memory model) | possible: same family as 6's problem | none: Cat Mail right without it (0030) |

## On the committed IPA

Dev IPA `7bc21c14cd6c256dbbaff70ee9432b9a476dcb35c4680203ec6ed98f60215e1d` (commit `58534bb`),
installed in place; every run's `installed` event names it:

- **Cat Mail Co. Demo**, Vulkan, 16:9, no graphics options: first frame +4.8 s; through the pad
  the tutorial warehouse draws as on DXMT (barred counters, shelves with folders and yarn, the
  chequered floor, the lit window), and a step forward redraws it from closer (`…T234121`).
  Uninstalled through the UI afterwards (`…T235955`).
- **Hollow Knight** (`app-367520`), Vulkan, `until first-frame+10 --shot`: first frame +10.5 s,
  title menu (`…T235307`). An earlier run in the same session (`…T234440`, first frame +11.1 s,
  frames presenting by its `[frames]` lines) took an all-black screenshot, as did the
  `os26_workarounds` launch after it; both reruns show the menu.
- **Portal 2** (`app-620`), Vulkan, `until first-frame+10`: first frame +6.3 s, the Source start
  screen (`…T234531`).

## Follow-ups

- Why vertex-stage descriptor loads go stale on iOS 27 (CPU-written descriptor memory, a missing
  cache invalidation between submissions, or how KosmicKrisp places descriptor sets): a real fix
  would let workaround 6 go entirely.
- Fragment shaders keep plain descriptor loads; a title whose fragment shaders read stale
  descriptors would show a similar fault on its surfaces. Try `MESA_KK_DEBUG=os26_workarounds` on
  such a title first.

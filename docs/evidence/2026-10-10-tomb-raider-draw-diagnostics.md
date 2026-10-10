# Tomb Raider: draw diagnostics, black picture remains

**Linear:** [PLA-150](https://linear.app/playportdev/issue/PLA-150/tomb-raider-203160)

The owner confirmed the black screen and authorized taking over the unfinished
investigation. This is diagnostic work, **not a rendering fix**.

## Diagnostic repair

Mesa patches 0020 and 0021 add opt-in `MESA_KK_DEBUG=draws` and `solid`, passed
through the game's existing Graphics options UI. Without the flags they do not
log draws or replace fragment outputs. `solid` changes the shader cache key.

The inherited vertex diagnostic failed compilation: `kk_debug_record_memory`
was used before declaration. Repaired that and replaced its stale CPU-pointer
registry with a locked, device-scoped live-allocation registry. Memory is removed
before free; reads copy bounded snapshots while holding the registry lock and
check ranges by subtraction. Snapshots describe CPU-visible data during command
recording, not an independently captured GPU execution. The registry is bounded
at 4096 live allocations; an untracked allocation yields no CPU view.

`draws` logs up to 400 draws and 300 buffer descriptors. Up to 60 blended,
non-indexed direct draws additionally show the first four vertices' first 16
bytes as floats (bounded by stride and vertex count). Packed colour bytes may
print as NaN; that alone does not mean corrupt coordinates.

## Verification

Dev IPA sha256:
`afd44b194a6d8d6c1a43a7a665d9815a24e83840f2fd782141df67a19f242727`.

- `./pp test --quick`: passed (Swift tests omitted).
- `./pp build`: passed, 80 IPA verification checks. Installed as an in-place
  upgrade, keeping the container.
- Tomb Raider, `-nolauncher`, `MESA_KK_DEBUG=draws`:
  `.work/agent-notes/tomb-raider/resume-vtx`, first frame +4.05 s, run through
  first-frame+25. Screenshot remains black. The runner's success means the
  requested wait completed, **not** that the game renders correctly.
- Hollow Knight, flags off:
  `.work/agent-notes/tomb-raider/resume-hk`, first frame +10.03 s, through
  first-frame+10. Screenshot shows the normal main menu with touch controls.

## Lead, not a diagnosis

Tomb Raider submits blended UI draws. One direct draw starts at vertex 13,
stride 32, count 198. Its first three position floats include
`(-17877.7, -28082, 0)`, `(8789, -28082, 0)` and
`(-17877.7, 51918, 0)`. Other text-like direct draws show similarly large
coordinates. These require comparison with the desktop and the transforms;
this evidence does not establish a malformed vertex buffer or a GPU defect.

Existing, earlier experiments retained under `.work/agent-notes/tomb-raider/`:

- `desktop/runs/proton-dxvk/game-060.png`: the desktop game's terms-of-service
  text is visible. No acceptance or bypass was performed in this takeover.
- `tr-solid/screen-stop.png`: forced fragment colour produces broad magenta
  coverage. It does not establish that individual text glyphs rasterize correctly.
- `tr-msl/pull/s1-host.log`: generated Metal shaders for further comparison.

Tomb Raider remains **Broken**. The desktop reference and vertex snapshots
narrow the next comparison; the cause and a phone-verified fix remain open.

## Desktop comparison and font/AFP checks

Scratch DXVK instrumentation at `D3D11CommonContext::Draw` on the isolated sway
display logs the first four vertices once per distinct draw count (stride 32).
The game files are the earlier phone copy; DXGI and the other overlay DLLs are
from the IPA, while d3d11 is the scratch instrumented build. This is not a
production DLL change or a phone verification.

- `desktop/runs/vertex-reference` reached the menu/DLC notice using existing
  scratch saves. Therefore its screen was not a matched terms-screen reference.
- `desktop/runs/vertex-terms-distinct` temporarily moved only that scratch
  prefix's saves aside, restoring them afterward. The screenshot shows readable
  terms text, without accepting it. A 198-vertex draw has a first glyph quad
  approximately 160 by 280 units, versus 26667 by 80000 in the phone trace.
  Draw counts 222, 156 and 1152 also show small quads on the desktop. Counts alone
  do not prove identical draw identity; the aspect/resolution also differs.
- Phone `resume-font`, same IPA, `env.WINEDEBUG=-all,+font` through the existing
  Runtime options UI: first frame +4.13 s through +15. FreeType 2.14.3 loads;
  system-font metrics are ordinary. The calls include the game's window title,
  not a proven trace of its embedded UI glyph rasterization. Vertex snapshots
  are unchanged. No matching font failure was established.
- Phone `resume-noafp`, same IPA, `env.FEX_HOSTFEATURES=disableafp` through Runtime
  options: FEX logs AFP=0; first frame +4.21 s through +20. The oversized vertex
  snapshots remain unchanged. Disabling AFP is not a fix.

The desktop samples are at the D3D11 call boundary; the initial phone samples
are at Vulkan command recording. DXVK patch 0002 (`DXVK_DEBUG_VERTICES=1`,
Runtime options) compares the same boundary before assigning
the corruption to game/CPU execution rather than buffer translation. Its scope
is immediate-context direct draws, mapped buffer 0, stride 32, counts 11..8191,
four vertices once per distinct count; it logs position, packed colour as float,
and UV. It is off by default.

## Matched phone boundaries

Dev IPA `0b10582ba7db63f42358838ede844ef9b9e867892899b55cd6d4907d8a72a97e`:
`pp test --quick` passed; build and all 80 IPA checks passed. Installed in place.

- `resume-d3d11`: first frame +4.09 s through +20, screenshot black. Both
  `[dxvk-vtx]` (D3D11 Draw) and `[kk-vtx]` (Vulkan command recording) show the
  same oversized positions. For the 198-vertex draw, the first glyph UV spans
  (0.00195312, 0.000976562) to (0.00976562, 0.00488281), only a tiny atlas region.
  The anomalous geometry already exists before Vulkan submission; it is not
  introduced by KosmicKrisp reading the vertex buffer.
- `resume-d3d11-hk`: diagnostic off, first frame +9.75 s through +10; screenshot
  shows Hollow Knight's normal main menu and touch controls.

The cause upstream of the draw remains open. A desktop A/B with the phone's
C runtime is the next comparison, not a claim that the CRT is responsible.

## Ruling out the CPU translation

- Desktop `desktop/runs/vertex-phone-ucrt-fallback`: the IPA's i386
  `ucrtbase.dll` (Wine's, as on the phone) loaded for the game
  (`+loaddll`: `game\ucrtbase.dll ... builtin` at 7B7B0000); the 198- and
  1152-vertex draws keep the desktop's small quads (15.67,59 to 175.67,339).
  The phone's CRT does not produce the geometry.
- fex 0023 (diagnostics) honours FEX_HOSTFEATURES' disable switches for the
  features the iOS port sets by hand, and logs the set. Dev IPA
  `9e0b5e6afb943002feb92acc00c95d6b53f0565d3a1d61982faced15ab00c48e` (80
  checks). Phone runs `ab-base`, `ab-noflagm`, `ab-noatomics`, `ab-norcpc`,
  `ab-nofcma`, `ab-nocrypto`, each through Runtime options and logged by FEX
  as applied (FlagM/FlagM2, LSE atomics, RCPC/LRCPC2, FCMA, AES/CRC/SHA/PMULL
  off in turn): the first draw's vertices are bit-identical in all six
  (-17877.7,-28082 ...). Together with the earlier AFP off, full x87,
  vector TSO/memcpy and MaxInst=1 runs, the geometry is deterministic and
  not a product of one FEX code path.
- The game's own log (`phone-gamelog/Tomb Raider.log`) differs from the
  desktop's only in machine facts: the adapter (Apple A19 Pro GPU, vendor
  106b), 6 logical CPUs, 8 GB RAM and 3.1 GB free virtual memory, and the
  online web service retries. No error.

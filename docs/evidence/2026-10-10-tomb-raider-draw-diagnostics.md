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

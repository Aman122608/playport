# 0086: Steam's DirectX setup is skipped, not run

**Status:** accepted, 2026-10-11, by the owner: "if it really does and our dlls already cover what it
installs let's make it skippable and don't try to fix it" (relayed by the compatibility speedrun's
supervisor; the implementation's choices are the worker's conservative ones, owner away);
[PLA-174](https://linear.app/playportdev/issue/PLA-174),
[PLA-170](https://linear.app/playportdev/issue/PLA-170),
[PLA-158](https://linear.app/playportdev/issue/PLA-158). Amends
[0081](0081-steam-common-redistributables.md) ("each listed `Run Process` entry runs", "which
depots") and [0083](0083-prerequisite-rerun-per-build.md)'s consequence that DXSETUP reruns once per
build. [Evidence](../evidence/2026-10-11-dxsetup-skipped.md).

## Context

Steam's DirectX June 2010 redistributable (app 228980's depot 228990, `DXSETUP.exe /silent`) cannot
succeed on iOS: every one of its x64 cabs carries an `infinst.exe` fixed at `0x100000000`, below the
lowest address a PIE app can map ([PLA-174 evidence](../evidence/2026-10-11-fixed-base-4g.md)). Under
0083 it ran once per new app build, and under [0084](0084-prerequisites-before-a-restart.md) each run
cost a restart before the game, for every Steam title that lists DirectX.

Under Proton it has no lasting effect: Proton's `proton` script copies its builtin DLLs over what
DXSETUP installed at every launch (`builtin_dll_copy`, commented `#dxsetup redist`:
`d3dcompiler_*`, `d3dcsx*`, `d3dx*`, `dx8vb`, `x3daudio*`, `xactengine*`, `xapofx*`, `xaudio*`,
`xinput*`). Playport's session re-links its runtime DLLs before each launch the same way
(`link_system32`).

## Decision

- **Which entry.** A Steam install-script `Run Process` entry whose every step runs `DXSETUP.exe`
  from a `_CommonRedist\DirectX` folder (names matched without case), from the 228980 depot or a
  game's own depot (PlayportKit `RunProcess.isDirectXSetup`).
- **Not run.** It is not passed to the launch: the library logs
  `prerequisite: Steam DirectX (<HasRunKey>) skipped: Playport's built-in DirectX libraries cover it`,
  records it in `playport-prerequisites.json` as `skipped` (with that reason, no exit code), and
  writes its HasRunKey value (the entry's name = 1, Steam's form) into
  `playport-registry-system.reg` before the runtime starts, so the game sees it installed in that
  Play. A recorded failure is replaced and never reruns; once the hive holds the value, the entry
  counts as run by Steam's own check.
- **Not downloaded.** Depot 228990 (DirectX June 2010, whose one entry is DXSETUP) is left out of the
  228980 download; when a title lists it and Playport does not hold it, its known script (one entry,
  `dxsetup`, HasRunKey `HKLM\Software\Valve\Steam\Apps\CommonRedist\DirectX\Jun2010`) gives the same
  skip. A held depot's own script is read as before and gives the same skip.
- **No restart.** Nothing ran, so 0084's restart does not happen.

## Why

It is the owner's call, and Proton parity in effect: Proton's copy makes the builtins win whatever
DXSETUP did. Fixing DXSETUP would need a smaller `__PAGEZERO` or relocation reconstruction, both
rejected or deferred in the PLA-174 evidence, for an installer whose output Proton discards.

## Consequences

- Names DXSETUP installs that neither Proton nor Playport ships as a builtin (`d3dcsx_42`,
  `d3dcsx_43`, `d3dx10` (the December 2006 one), `xactengine2_1`/`2_2`/`2_3`/`2_5`/`2_6`/`2_8`/`2_10`,
  `xapofx1_0`, and the x86-only Managed DirectX 1.1 assemblies `Microsoft.DirectX*.dll`) keep a
  native copy under Proton and are absent here. A game that loads one fails to find it; none is
  added by this record.
- Steam's DirectX titles start on their first Play without the DXSETUP run or a restart, and
  without the 103 MB download.

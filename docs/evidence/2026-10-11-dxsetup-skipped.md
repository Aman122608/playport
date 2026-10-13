# 2026-10-11: Steam's DirectX setup skipped (PLA-174, PLA-170)

**Linear:** [PLA-174](https://linear.app/playportdev/issue/PLA-174),
[PLA-170](https://linear.app/playportdev/issue/PLA-170),
[PLA-158](https://linear.app/playportdev/issue/PLA-158).
**Decision:** [0086](../decisions/0086-directx-setup-skipped.md).
**Question:** do Playport's builtins cover what DirectX June 2010's DXSETUP installs, and does a
Play skip it, write its HasRunKey and start the game without a restart?
**Result:** the builtins cover the same names Proton's do (one list of gaps, below). Castle
Crashers Demo's Play skipped DXSETUP, wrote the HasRunKey, did not restart and reached its first
frame in 4.4 s.

## Coverage

The DLLs of every cab in Steam's `_CommonRedist/DirectX/Jun2010` (depot 228990, 154 cabs: 75 x64,
78 x86 with Managed DirectX's among them, and DXSETUP's own `dxupdate.cab`), listed with
`cabextract -l` from the copy on the phone, compared with `app/artifacts.tsv` (`Runtime/i386-windows/` for x86, `Runtime/arm64ec-windows/` for x64) and
with Proton 10.0's `files/lib/wine/{i386,x86_64}-windows/`:

- **Covered in both architectures:** `d3dcompiler_33`–`43`, `d3dx9_24`–`43`, `d3dx10_33`–`43`,
  `d3dx11_42`/`43`, `x3daudio1_0`–`1_7`, `xactengine2_0`/`2_4`/`2_7`/`2_9`, `xactengine3_0`–`3_7`,
  `xapofx1_1`–`1_5`, `xaudio2_0`–`2_7`, `xinput1_1`–`1_3`, `xinput9_1_0`.
- **Not covered:** `d3dcsx_42`, `d3dcsx_43`, `d3dx10` (December 2006), `xactengine2_1`, `2_2`, `2_3`,
  `2_5`, `2_6`, `2_8`, `2_10`, `xapofx1_0`, and the x86-only Managed DirectX 1.1 assemblies
  (`Microsoft.DirectX.dll` and its eight `Microsoft.DirectX.*` companions). Proton 10.0 ships none of
  them either, so its `#dxsetup redist` copy leaves DXSETUP's native copy there. Playport has no copy;
  no DLL was added (decision 0086).

## Phone

iPhone18,4, iOS 27.0, dev IPA `b392cd08e5a4fed51d7d3dfa9f90feaefc54e90dcc760978f410b1dfcac4f38e`
(2dff9fe), `pp ui --wait 180`. Run directories are under `.work/ui-runs/`. The phone's prefix held
Castle Crashers' DXSETUP as a failure (0xc0000018) from an earlier build. Steamworks Shared already
held depot 228990, so its own script was read: the path that leaves the depot undownloaded is
covered by the host tests only.

- **Install** Castle Crashers Demo (`install:207100`): `20261011T132543`.
- **Play** `app-207100 --until first-frame+10 --shot`: `20261011T132626`.

      library: prerequisite: Steam DirectX (HKEY_LOCAL_MACHINE\Software\Valve\Steam\Apps\CommonRedist\DirectX\Jun2010) skipped: Playport's built-in DirectX libraries cover it
      library: steam: Castle Crashers Demo install script registry: Software\Valve\Steam\Apps\CommonRedist\DirectX\Jun2010 "dxsetup"
      title: +3.43 s game started
      title: +4.40 s first frame

  No `title: prerequisite` line, no `launch=restart`, no redistributable download. The screenshot
  shows the title's Press Start screen.
- **Afterwards** the prefix's `system.reg` holds
  `[Software\\Valve\\Steam\\Apps\\CommonRedist\\DirectX\\Jun2010]` `"dxsetup"=dword:00000001`, and
  `playport-prerequisites.json` records its id as `skipped` with that reason, no exit code.
- **Uninstalled** (`uninstall:app-207100`): `20261011T132803`.
- **Hollow Knight** (`app-367520`): first frame at 9.0 s, main menu: `20261011T132840`.
- **Portal 2** (`app-620`): first frame at 6.1 s, Source intro: `20261011T132940`.

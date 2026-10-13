# Evidence: GOG's Inno Setup needs code page 1250; NWN Diamond's support setup completes (PLA-233, PLA-187)

**Date:** 2026-10-12. **Kind:** desktop Wine runs on the workstation, a read of the setup's
binary, phone runs (dev app). **Linear:** [PLA-233](https://linear.app/playportdev/issue/PLA-233),
title [PLA-187](https://linear.app/playportdev/issue/PLA-187) (Neverwinter Nights Diamond, GOG
1207658890). **Changes:** `build/stages/stage-artifacts.py` (the NLS set), `app/artifacts.tsv`.
The owner was away, so the conservative option was taken at each choice below. Desktop scratch is
under `$PLAYPORT_BUILD/agent-notes/sweep11b/`; run directories under `$PLAYPORT_BUILD/ui-runs/`.

**IPA:** `Playport-26.5-13f8dda9.ipa` (dev, sha256
`13f8dda954c39a5e3ea5999ecd457015b97ccef906984b8837485d475b1b43f7`), built from 3b942dd with this
change and `patches/fex` 0031 ([2026-10-12-x87-m64-across-blocks](2026-10-12-x87-m64-across-blocks.md)),
installed in place.

## The cause: not FEX, a missing code page

[2026-10-12-gog-support-setup](2026-10-12-gog-support-setup.md): with the session at 80-bit x87,
the setup (`galaxy_nwn_diamond_2.1.0.21.tmp`, Inno Setup 5.5.5 Unicode, Delphi) ended with
`Runtime Error (at -1:0): Cannot Import EXPANDCONSTANT.` and exit code 1. Its log on the phone
(`s11-nwn-x87`) also has 406 failed `NtGetNlsSectionPtr` calls for code page 1250 (`c0000034`),
from the setup process alone.

- Inno registers each script function by name from a table of AnsiString declarations
  (`ScriptFunc_R.pas`: `AnsiString(ExtractName(String(Decl)))`); PascalScript's `LoadData` then
  binds each import by name and hash. Inno 5.5.5 has about 200 such declarations, two conversions each.
- A Delphi 2009+ AnsiString literal carries the code page of the machine that compiled it. In the
  setup's binary 934 of the 939 short AnsiString literals name code page 1250 (Central European),
  for example the one before `function ExpandConstant(const S: String)`: this build was compiled
  on a Polish Windows.
- The app shipped only `c_1252`, `c_20127`, `c_28591` and `c_437` (Madeira's set). Without
  `c_1250.nls` each conversion returns an empty string, every function is registered with an
  empty name, and the first import, `EXPANDCONSTANT`, fails.

Desktop Wine A/B (Wine 11.18, the same setup and command line, scratch prefix): with the system's
NLS files the setup installs; with a copy of Wine whose data directory and prefix lack
`c_1250.nls` (`IsValidCodePage(1250)` then returns 0, as on the phone), it logs
`Cannot Import EXPANDCONSTANT.` and exits 1. So the 80-bit x87 failure was this one, and it does
not depend on FEX.

## The change

`stage-artifacts.py` adds every other Windows ANSI code page to the bundle's `nls/`: 874, 932,
936, 949, 950, 1250, 1251, 1253 to 1258 (1.3 MB). Those are the code pages `GetACP` can return
on Windows, so a program's literals can carry any of them. The conservative bound: only 1250 was
seen failing, but its neighbours are the same case for a program built elsewhere. OEM, Mac
(`c_10000`, asked for 20 times by each session's start, without effect) and ISO pages beyond
those shipped are not added.

## Phone runs on `13f8dda9…`

| Run | What | Outcome |
| --- | --- | --- |
| `20261012T170848` | NWN Diamond, first Play on this build, default settings (`x87reduced=1`) | the support setup ran as a prerequisite (0099) and exited `0` after about 1 s; no box. The folder now has `binkw32.dll` (358,963 bytes, the full DLL, not the ATI shim), `nwn.ini`, `nwncdkey.ini`, `nwnpatch.ini`, `nwnplayer.ini`, `nwtoolset.ini` and `unins000.*`. Playport restarted for the game (the setup had used the JIT pool's head), and `nwmain.exe` loaded `binkw32.dll`, created an 800×600 OpenGL window through Zink (`zink Vulkan 1.4 (KosmicKrisp)`, core 3.3) and audio streams: first frame +5.8 s, ran to first frame +30 s. The screen was black at the stop |
| `20261012T171052` | the same, second Play | the setup was skipped (recorded `exit 0x0`); first frame +5.5 s, ran to first frame +75 s, still black. A thread sample shows the main thread at about 20 % CPU in `binkw32.dll` (guest `0x7b3dd6d5`, base `0x7b3b0000`) making server calls, the wined3d command-stream thread waiting |

## Outcome

PLA-233 is fixed: the setup's script runs and completes on the phone at the default x87 setting.
PLA-187's missing `binkw32.dll` is fixed: NWN Diamond no longer exits `0xc0000135`. It is still
not playable: it shows nothing through first frame +75 s. Its intro is Bink video (`movies/`), and
the main thread is busy in `binkw32.dll`; whether Bink waits for an audio clock, draws through
DirectDraw (wined3d over OpenGL) to a surface that is not shown, or something else was not traced.
Next step, a follow-up: one Play with Escape or A pressed during the intro to see whether the menu
appears, then the Bink path (a `WINEDEBUG` of `ddraw` and `dsound` for one session, or the same
title under desktop Wine).

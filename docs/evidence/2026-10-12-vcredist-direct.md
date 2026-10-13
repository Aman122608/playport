# 2026-10-12: The installer named by Steam's standard VC++ `.cmd` runs directly (PLA-212)

**Linear:** [PLA-212](https://linear.app/playportdev/issue/PLA-212) (follows
[PLA-209](https://linear.app/playportdev/issue/PLA-209)).
**Decision:** [0096](../decisions/0096-steam-outcome-parity.md), amending
[0081](../decisions/0081-steam-common-redistributables.md) and
[0082](../decisions/0082-system32-programs.md).

**Questions:**
- Does a Steam VC++ `.cmd` entry in its standard form now run its installer with no `cmd`,
  `conhost` or `start` and no console window?
- What does it then cost in JIT pool head?
- Does the game still start, in the same session?

**Result:**
- Counter-Strike's VC++ 2019 x86 entry ran `VC_redist.x86.exe /q /norestart` directly. It started
  two processes, Burn and its `.cr` clean-room copy. No `cmd`, `conhost` or `start` ran, and no
  window was shown before the game.
- The head rose by 13.9 MiB (19.5 → 33.3), against 99.2 MiB through the `.cmd` (0094's evidence).
- The game started 0.28 s after the entry ended, in the same session, with 462 MiB of room for it,
  where the same entry through the `.cmd` left 377.

## The script forms found

| Where | Script | Text |
| --- | --- | --- |
| Steamworks Common Redistributables, VC++ 2019 depot (228988), as on the phone in `Games/Steamworks Shared/_CommonRedist/vcredist/2019` | `Microsoft Visual C++ 2019 x86.cmd` | `@ECHO OFF` / `start /w "" "%~dp0\VC_redist.x86.exe" /q /norestart` / `IF %ERRORLEVEL% == 3010 EXIT /B 0` / `EXIT /B %ERRORLEVEL%`, with CRLF line ends, 121 bytes |
| the same depot | `Microsoft Visual C++ 2019 x64.cmd` | the same, with `VC_redist.x64.exe` |

- These are the only VC++ `.cmd` files in the build area or on the phone. Their scripts are the
  same template, with only the file name differing.
- The phone holds no other 228980 depot now. Lossless Scaling's VC++ 2022 entries (2026-10-11)
  ran `.cmd` scripts of the same names, but their texts were not kept.
- The unit tests use both texts byte for byte (`SteamVCRedistCmdTests`).

## The matcher

PlayportKit `SteamVCRedistCmd.match`. A script matches only when its one effective command is
`start /w "" "%~dp0\<installer>" <arguments>`. Its bounds:

- **Allowed lines:**
  - before the command: blank lines, `@echo off`, and `rem` and `::` comments;
  - after it: blank lines, comments, `if %errorlevel% == 3010 exit /b 0` and
    `exit /b [%errorlevel%]`.

  Case and line endings do not matter. Any other line means no match.
- **The command:** `start` needs `/w` (or `/wait`) and the empty title `""`.
- **The installer:**
  - It is beside the script (`%~dp0`, with or without the `\`), not in a subfolder, so the
    working directory is the script's folder, as `cmd` gives it to `start`.
  - It is named `vc_redist…` or `vcredist…` and ends `.exe`.
  - It must be present (matched without case, as the guest sees it).
- **The arguments:** plain words. Quotes, `%`, `!`, `&`, `|`, `<`, `>`, `^` and parentheses mean
  no match.
- **The entry:** its `command 1` must be empty.

Anything else runs through `cmd` as before. The tests cover these cases:
- no `/w`; no title;
- a subfolder; an absolute path; `..`;
- a .NET installer; a `.cmd` target;
- `%1`; a quoted argument; a redirection; `&`;
- a command before or after the start;
- another code mapped; the code dropped (`exit /b 0`);
- two starts;
- a missing installer.

**Exit codes.** Run directly, the installer's own code is recorded. Through the `.cmd`, 3010 would
be recorded as 0. Both are successes under 0083, and every other code passes through the script
unchanged, so the outcome is the same for every code.

## Phone

All runs are on the iPhone18,4 with iOS 27.0, using the dev IPA
`b74365c0e16e690d6752a5e5d4bd27a564a96d16333995cd6a3c363fa26a2845`, built and installed by
`pp install` from 825c102. That is the change; this record's commit only adds documentation.
Plays use `pp ui --wait 180`. Run directories are under `.work/ui-runs/`.

### Counter-Strike (`app-10`), the x86 entry run directly: `20261012T093429`

The prefix's record had the 2019 x86 entry failing (0x666) under another build. The new build
therefore ran it again (0083). The x64 entry had succeeded before and was skipped, since its
HasRunKey is in the prefix.

```
title: prerequisite: Steam x86 14.28.29334.0 (…2019/installscript.vdf) failed with another build (…|x86 14.28.29334.0 exit 0x666 by build 32c31bea035df32e); build 959d3edbbc447584 runs it again: C:\Games\Steamworks Shared\_CommonRedist\vcredist\2019\Microsoft Visual C++ 2019 x86.cmd
title: +4.74 s prerequisite started
title: prerequisite: direct (standard VC++ .cmd) C:\Games\Steamworks Shared\_CommonRedist\vcredist\2019\VC_redist.x86.exe /q /norestart for C:\Games\Steamworks Shared\_CommonRedist\vcredist\2019\Microsoft Visual C++ 2019 x86.cmd
session: prerequisite 1: C:\Games\Steamworks Shared\_CommonRedist\vcredist\2019\VC_redist.x86.exe in C:\Games\Steamworks Shared\_CommonRedist\vcredist\2019, 2 arguments, 12 variables
session: prerequisite 1: ended, exit code 666 after 770 ms
title: prerequisites: 1 ran, room_for_game_mb=462 (needs 320); the game starts in this session (decision 0094)
title: at the game's start: pool: size_mb=512 head_mb=34 head_live_mb=20 head_free_mb=14 tail_mb=49 tail_live_mb=17 room_mb=430 … children=0 … exhausted=none
title: +6.07 s game started
title: +6.40 s first frame
[guest-window] the game presents: 0 window(s) were shown; …
```

- The session's `NtCreateUserProcess` images were these three, and no others. No log line names
  `conhost`, `cmd.exe` or `start.exe`.
  - `…\2019\VC_redist.x86.exe`
  - `C:\windows\temp\{…}\.cr\VC_redist.x86.exe` (Burn's clean room)
  - `C:\Games\Half-Life\hl.exe`
- Before the game presented, no guest window was shown: no console, and Burn's `/q` shows no
  window.
- **Head:**
  - The cursor was at 19.5 MiB (`used=0x1374000`) before the request.
  - It was at 33.3 MiB (`head_used=0x2154000`) at the game's first tail allocation.
  - So the entry took 13.9 MiB. Through the `.cmd` it took 99.2 MiB (`20261012T085407`, the same
    entry and exit code).
- At `mark:game started+20` the pool read `head_mb=41 tail_mb=177 tail_live_mb=161 room_mb=295
  exhausted=none`.
- The screenshot shows Counter-Strike's main menu (New game, Find servers, Options, Quit).

### A fresh x86+x64 pair: computed, not run

Nothing in the UI resets a prerequisite's record. Every VC++ HasRunKey the phone's 228980 depots
offer is already in the prefix: 2019 x64 and 2022 x86/x64 succeeded, and 2019 x86 reruns per build
with 0x666. The build area's records name no small owned title that lists another VC++ `.cmd` depot. So a fresh pair
could not be had through the UI, and its pool is computed from the measurements:

- **x86 entry:** 13.9 MiB, measured above (Burn plus `.cr`).
- **x64 entry:** about 22 MiB. Its `.cmd` run took 106 MiB, less the same 84 MiB chain. Burn,
  `.cr` and `.be` are i386 processes of about 7 MiB each.
- **The pair:** about 36 MiB of head over the session root's 20. That leaves about
  512 − 56 − 17 ≈ 439 MiB of room for the game, against 263 through the `.cmd`s. That is above
  0094's 320 MiB, so **a fresh pair no longer restarts**.

**Caveat.** These measured runs ended with 0x666 in under a second. A real first install may start
more processes, for example a Windows Installer custom-action server, which is not measured here.
Each i386 process costs about 7 MiB and an x86-64 one about 12 MiB or more. Even four more x86-64
processes would leave more than 380 MiB.

### Gates

| Title | Run | First frame | Screen |
| --- | --- | --- | --- |
| Hollow Knight (`app-367520`, `--until first-frame+10 --shot`) | `20261012T093614` | +10.03 s | main menu (Start Game, Options, Achievements, Extras, Quit Game); pool head 82, tail 161, room 270, exhausted none |
| Portal 2 (`app-620`, `--until first-frame+10 --shot`) | `20261012T093710` | +5.77 s | the Source splash; pool head 27, tail 161, room 325, exhausted none |

No runtime code changed, so there was no `pp perf` run.

## 0x666

0x666 is 1638, `ERROR_PRODUCT_VERSION`: "another version of this product is already installed".
Here the newer VC++ 2022 x86 runtime (14.51) refuses the 2019 x86 (14.28) bundle.

0083 treats 1638 as a failure on purpose, "as Steam's own scripts treat it": the `.cmd` maps only
3010 to 0. Steam on Windows would retry such an entry at every launch until its HasRunKey is
written. Playport retries once per app build. That rule is unchanged. It now costs about 14 MiB of
head and under a second per new build, not 99 MiB.

## Host checks

- PlayportKit `SteamVCRedistCmdTests`:
  - `testSteamsVCRedistScriptsMatchWithTheirInstallerAndArguments`
  - `testCommentsBlankLinesCaseAndLineEndingsDoNotMatter`
  - `testAnythingElseRunsThroughCmd`
  - `testAStandardVCRedistEntryRunsItsInstallerDirectlyAndAnyOtherCmdThroughCmd`
  - `testAStandardScriptWhoseInstallerIsMissingRunsThroughCmd`
- The existing `SteamInstallScriptTests`, with empty `.cmd` files, still run through `cmd`.
- `pp test --quick` and `pp test --swift PlayportKit` (224 tests) pass.

## Not checked

- A fresh x86+x64 pair on the phone (computed above).
- A real first install of a VC++ redistributable run directly. The runs above were refusals of an
  older version.
- The release variant on the phone. Owner sign-off applies (decision 0068).

# 0096: Prerequisites match Steam's outcome, not its process chain; a standard VC++ `.cmd` runs its installer directly

**Status:** accepted, 2026-10-12. The owner gave the rule: "I need outcome to be on par with
steam/proton, not the exact process". The compatibility speedrun's worker applied it with the
owner away, taking the conservative option at each choice; those choices are recorded for review.
[PLA-212](https://linear.app/playportdev/issue/PLA-212).
Amends [0081](0081-steam-common-redistributables.md)'s "each listed `Run Process` entry runs"
and [0082](0082-system32-programs.md)'s "a `.cmd` prerequisite now really runs `cmd`, `conhost`,
`start` and the installer's processes". It also answers the follow-up that
[0094](0094-restart-only-when-the-pool-is-short.md) left open ("run the installer of Steam's
standard VC++ `.cmd` directly").
[Evidence](../evidence/2026-10-12-vcredist-direct.md).

## Context

0081 runs each Steam install-script entry as Steam lists it. 0082 made a `.cmd` entry run that way:
Steam's VC++ 2015–2022 scripts are

```
@ECHO OFF
start /w "" "%~dp0\VC_redist.x86.exe" /q /norestart
IF %ERRORLEVEL% == 3010 EXIT /B 0
EXIT /B %ERRORLEVEL%
```

That chain has costs on iOS:

- **JIT pool.** It makes three x86-64 processes before the installer: `cmd`, `conhost` and
  `start`. Together they take about 84 of the entry's 99 MiB of JIT pool head (0094's evidence).
  A fresh x86 and x64 pair therefore leaves the game 263 MiB, under 0094's 320, and so it
  restarts Playport.
- **A console window.** `conhost` shows one. Before the first frame it is the launch's first
  guest window (0075, PLA-214).

The installer itself, a WiX Burn bundle, takes 14–20 MiB.

0094 kept the chain because running the installer directly "would interpret batch files and
depart from running what Steam runs". The owner has since settled that what counts is the outcome.

## Decision

- **The rule: Playport matches Steam/Proton's outcome for a prerequisite, not its process
  chain.** The outcome is:
  - the same redistributables installed, by the same installers;
  - the same arguments and working directory;
  - the same exit-code handling: 0083's success codes, and the HasRunKey 0081 writes.

  The processes in between may differ when that outcome stays the same.
- **The first application: Steam's standard VC++ `.cmd`.** When a Steam install-script entry's
  process is a `.cmd` whose only effective command is `start /w "" "%~dp0\<installer>" <args>`,
  Playport runs that installer directly, with those arguments, as the entry's prerequisite. The
  matcher is PlayportKit `SteamVCRedistCmd`. These are its conservative bounds:
  - **Allowed around the command:** blank lines, `@echo off`, and `rem` and `::` comments. After
    it, `if %errorlevel% == 3010 exit /b 0` and `exit /b [%errorlevel%]` are also allowed. Any
    other line means no match.
  - **The installer** is a file beside the script (`%~dp0`, with no subfolder). So its working
    directory is the script's folder, the one `cmd` gives `start`. It is named `vc_redist…` or
    `vcredist…` and ends `.exe`, which covers what was measured. It must be present.
  - **The arguments** are plain words: no quotes, variables, redirections or command separators.
    The entry itself passes the script no arguments (`command 1` is empty).
  - **Anything else runs through `cmd` as before.** That covers another script form, another
    installer and the .NET scripts.
- **The prerequisite keeps its identity.** It has the same id (`HasRunKey|entry`), record, rerun
  rule (0083), HasRunKey write (0081) and launch-screen step (0095).
- **Exit codes.** Under `cmd` the script exits with the installer's code, with 3010 turned into
  0. Run directly, the installer's own code is recorded. 0 and 3010 are both successes
  (`Prerequisite.successCodes`), and every other code passes through unchanged. So 0083's
  outcome is the same for every code, including 1638 (0x666, "a newer version is installed"),
  which stays a failure as 0083 decided. Only the number in the record differs, for a 3010.
- **The log says so:** `prerequisite: direct (standard VC++ .cmd) C:\…\VC_redist.x86.exe /q
  /norestart for C:\…\Microsoft Visual C++ 2019 x86.cmd`.

## Why

On Windows and under Proton, the result of Steam's VC++ entry is that VC++ runtime installed by
Microsoft's installer, with `/q /norestart`, and the installer's exit code read with 3010 as
success. `cmd` and `start /w` only carry the command line and the exit code. Running the
installer directly changes none of that. It saves about 84 MiB of JIT pool head per entry and
removes the console window. A fresh VC++ pair then costs about 30–40 MiB. That is well inside
0094's room, so the pair no longer restarts Playport.

A strict matcher keeps the shortcut to the scripts whose effect is fully known. A script that
does anything more still gets the real `cmd`, so no batch semantics are guessed.

## Consequences

- A Steam VC++ entry's console window no longer shows during its install. The launch screen's
  first guest window is then the game's, or the installer's own if it shows one (Burn's `/q`
  shows none).
- `cmd`, `conhost` and `start` in `system32` (0082) are still needed for other scripts, and for
  the titles and installers that call them.
- A later case under the same rule needs its own record: for example .NET's scripts, another
  script form, or another way to remove a process the outcome does not need. Each must state the
  outcome it keeps.

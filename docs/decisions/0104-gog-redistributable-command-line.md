# 0104: a GOG redistributable's arguments reach its installer as GOG Galaxy passes them

**Status:** accepted, 2026-10-13, by the sweep 2.11n worker with the supervisor's direction (the owner
away); [PLA-251](https://linear.app/playportdev/issue/PLA-251). Amends
[0090](0090-gog-redistributables.md)'s "the arguments are the repository's, as Heroic passes them".
[Evidence](../evidence/2026-10-13-gog-msvc2005-command-line.md).

## Context

GOG's repository gives each installer's arguments as one command-line string. Visual C++ 2005's
(`MSVC2005`, `MSVC2005_x64`) nests quotes for the IExpress package to read itself:
`/q:a /c:"VCREDI~3.EXE /q:a /c:""msiexec /i vcredist.msi /qb"" "`. Playport split every
prerequisite's string into arguments (CommandLineToArgvW's common cases) and the session root
quoted each again, so the package got `/q:a "/c:VCREDI~3.EXE /q:a /c:msiexec /i vcredist.msi /qb "`,
exited 0 in 0.3 s and installed nothing; the record called it a success, so it never ran again.

## Decision

- **As it is.** A GOG `.exe` redistributable's string follows the installer's quoted path unchanged
  (`Prerequisite.verbatim`, the session request `PP_REQUEST_PREREQUISITE_LINE`,
  `wine_host_session_prerequisite_line`, ABI 7), as GOG Galaxy passes it to CreateProcess. Other
  prerequisites (Epic's, Steam's install scripts, GOG `.msi` packages through msiexec, GOG support
  setups) keep their argument lists.
- **The record keeps the command line** each run used. A GOG string whose success was recorded with no
  or another command line runs once more; Steam's and Epic's records are not affected. A prefix that ran
  GOG redistributables before this therefore runs each once again (one restart, decision 0084).

## Consequences

- GOG's MSVC2005 installs Visual C++ 2005 SP1 (8.0.50727.762) into the prefix's WinSxS, as on Windows.
- Every GOG `.exe` redistributable already recorded as a success reruns once per prefix.

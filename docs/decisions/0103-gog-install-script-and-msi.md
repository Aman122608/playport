# 0103: a GOG install script's registry values are written from Play; `.msi` redistributables run through msiexec

**Status:** accepted, 2026-10-12, by the sweep 2.11k worker with the owner away (the conservative
option at each choice, recorded for review); [PLA-194](https://linear.app/playportdev/issue/PLA-194).
Takes up [0090](0090-gog-redistributables.md)'s "not covered" script interpreter and its left-out
`.msi` installers; leaves [0099](0099-gog-support-setups.md)'s generation-1 `support_commands` as
they were. [Evidence](../evidence/2026-10-12-gog-script.md).

## Context

Of the owner's 46 GOG titles, 45 have a generation-2 build and 43 of those say `scriptInterpreter:
true`, but only about a third of those (at least 15) ship a `goggame-<id>.script`; NWN Diamond's and
Spelunky's builds, without the flag, ship one too. GOG Galaxy runs it after an install with its
script interpreter (`__redist/ISI/scriptinterpreter.exe`, an i386 Inno Setup package that holds the
interpreter). The scripts read from the owner's installs and GOG's depots name five actions:
`setRegistry` (a root, a subkey and a value: `valueName`, `valueData` with `{app}`, `valueType`
`string` or `dword`; or a key alone, marked `saveGameData` or `deleteSubkeys`), `savePath` (cloud
saves), `installSDB` (an application-compatibility database), and `Execute` (a build's
`temp_executable`, which 0099 runs, or a support `.cmd`: Tomb Raider: Anniversary's sets Windows
Games Explorer keys). No readable script names a file or shortcut action. Tomb Raider:
Anniversary's (GOG 1202885143) writes `InstallPath`, `FullInstall` and `Version` under
`HKLM\Software\Crystal Dynamics\Tomb Raider: Anniversary`, which its i386 `tra.exe` reads.

GOG's dependency repository lists three `.msi` installers: `XNA` and `XNA_40` (`/qn`) and
`PHYSXLEGACY` (`/passive /norestart`), which 0090 left out. Dishonored's build names `PHYSXLEGACY`;
no owned build names XNA.

Only one owned title (Quake II, 1441704824) has a generation-1 build alone, and Playport cannot
install a generation-1 build ([PLA-188](https://linear.app/playportdev/issue/PLA-188)).

## Decision

- **Which scripts.** Each `goggame-<product>.script` at the root of the game's folder, as the install
  record lists them, at every Play (not on the restart after prerequisites). The build's
  `scriptInterpreter` flag is not consulted: a build without it has only an `Execute` of its
  `temp_executable` in its script, which this record does not run.
- **Which actions.** An action whose `languages` include the install's (`*`, `en-US`, `en`) and whose
  `osBitness`, if any, includes `64` (Playport installs 64-bit depots).
  - `setRegistry` with a value: written into the prefix through `PrefixRegistryFile`, as a Steam
    install script's `Registry` is (0081): before the runtime starts, a value the hive holds kept.
    `{app}` and `{supportDir}` are the game's folder (`C:\Games\<dir>`; Playport places support
    files there, 0099); another placeholder leaves that value out. `string` and `dword` (decimal or
    `0x`) only. An HKLM `Software` value goes into both registry views (`Software\…` and
    `Software\Wow6432Node\…`, not `Software\Classes`): GOG's interpreter is 32-bit, so on Windows
    its values land in the 32-bit view a 32-bit game reads, and the 64-bit view serves a 64-bit game.
  - Left out and logged: a key without a value (nothing to read; uninstall and cloud-save marks),
    `savePath` (GOG's cloud saves), `installSDB` (Wine has no shim engine), `Execute` (a
    `temp_executable` is 0099's; Tomb Raider: Anniversary's `.cmd` only registers it with Windows
    Games Explorer), and any other action.
- **Not GOG's interpreter.** Playport does not run `scriptinterpreter.exe`: the actions it would carry
  out that matter to a game are the registry values, and running an i386 Inno Setup package under
  FEX hit the failures recorded in 0099; the values written are the same.
- **`.msi` redistributables.** A repository entry whose installer is an `.msi` runs like an `.exe`
  one (0090: id `gog-redist:<id>`, once per prefix, from the game's folder after download):
  `C:\windows\system32\msiexec.exe /i C:\Games\<dir>\__redist\<id>\<file>.msi` plus GOG's
  arguments, with `/qn` added when they name no quiet or passive mode.
- **Generation-1 support commands:** not built. They need a generation-1 install first (PLA-188's
  design names them as its step 4).

## Why

The parity rule: a GOG game installed by GOG Galaxy or under Proton through a GOG client has its
script's registry values and its redistributables; Playport gives it the same values and runs the
same installers. Doing the registry part in Swift, as for Steam's install scripts, needs no i386
setup to run and is idempotent; the left-out actions change nothing a game reads in the prefix.

## Consequences

- A GOG game's script values are in the prefix from its next Play on; an older install needs no
  update (the script is in its folder).
- Writing into both registry views adds one unused copy per value for a game of the other bitness.
- A value the hive already holds is not updated, so a moved install keeps its old `InstallPath`
  until the key is removed, as for Steam's.
- `.msi` installs run with Wine's msiexec from system32; an MSI that needs a person waits for them
  within 0079's time limit.

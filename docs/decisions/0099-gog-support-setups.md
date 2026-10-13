# 0099: a GOG build's support setup runs once per build from Play, as GOG Galaxy and Heroic run it

**Status:** accepted, 2026-10-12, by the sweep 2.11 worker with the owner away (the conservative
option at each choice, recorded for review); [PLA-187](https://linear.app/playportdev/issue/PLA-187).
Takes up [0090](0090-gog-redistributables.md)'s "not covered" `temp_executable`, for generation-2
builds. [Evidence](../evidence/2026-10-12-gog-support-setup.md).

## Context

Neverwinter Nights Diamond (GOG 1207658890) exited at once with `0xc0000135`: `nwmain.exe` imports
`binkw32.dll`, which the build has only as a `support` file at `app/binkw32.dll` (with
`app/nwn.ini` and the other `.ini` files, and an ATI variant under `binkw32_ati/`). Nothing in the
game's folder root names it. The build manifest's `products[]` names a `temp_executable`,
`galaxy_nwn_diamond_2.1.0.21.exe` (the depot's one `support`+`executable` file), and its
`goggame-1207658890.script` has one action, `Execute` of that file. It is GOG's Inno Setup for
the game: under desktop Wine, with the command line Heroic passes, it copies `app/*` into the
folder (`binkw32.dll`, or the ATI shim and `binkw32_ati.dll` on an AMD GPU), writes the game's
BioWare and GOG registry keys and an uninstaller. Without `/ProductId` and `/galaxyclient` it
stops in `InitializeSetup`. GOG Galaxy runs the script after an install and an update; Heroic runs
each product's `temp_executable` with that command line.

The same build names its languages `en`, `fr`, …: the depot selection looked for `en-US`, found
none, and took the first other language's depot, French.

## Decision

- **Which.** For a GOG game, each installed product's (the base game's and owned DLCs')
  `temp_executable` from its build manifest, base product first; none for an empty value or GOG's
  `"None"`. Kept on the install record; an older record reads the base product's from its build
  once.
- **How it runs.** Through 0079's prerequisite mechanism, after the build's redistributables
  (0090), from the game's folder, where Playport places the depot's `support` files at their paths
  (the setup reads them beside itself). Id `gog-support:<product>@<build>`: once per prefix and
  build, so an update runs it again, as GOG Galaxy does; a failure runs once more per app build
  (0083). The command line is Heroic's (`/VERYSILENT /DIR=<folder> /Language=English
  /LANG=english /ProductId=<id> /galaxyclient /buildId=<build> /versionName=<version>
  /nodesktopshorctut /nodesktopshortcut /NOICONS`), plus `/SUPPRESSMSGBOXES`: nobody answers a
  message box in the middle of a Play. A setup not in the folder is left out, logged.
- **Not covered.** The script interpreter (`scriptInterpreter` builds, `__redist/ISI`), whose
  scripts have other actions; generation-1 builds' `support_commands` (Playport cannot install a
  generation-1 build: [PLA-188](https://linear.app/playportdev/issue/PLA-188)).
- **Language.** A depot language without a region (`en`) is the player's language with that
  primary tag (`en-US`), so NWN Diamond installs its English depot.

## Why

The parity rule: the game works under Proton through GOG's own client or Heroic, which both run
this setup; it is GOG's own installer, from the store's own channel, for this game. Copying `app/*`
into the folder in Swift would guess at a setup's contents (NWN's picks a DLL by GPU vendor and
writes registry keys the game may read). Once per build keeps an update's setup.

## Consequences

- A GOG game with a support setup runs it at its first Play (and after an update), a few seconds
  under the i386 route, with the "Installing dependencies…" screen (0095).
- The setup writes an uninstaller and `.lnk` files into the game's folder and the Start menu
  under the prefix; Playport's uninstall removes the folder, not the Start menu entries or the
  registry keys (as for 0090's redistributables).
- A setup that needs a person (a dialog it does not suppress) waits for them; the Play goes on
  after it.
- On the phone today NWN Diamond's setup does not complete: at the default reduced x87 precision
  its Inno loader corrupts its own path (a `fild`/`fistp` copy across FEX blocks) and shows a
  `Path not fpund.` box; at 80-bit x87 the setup fails to bind `ExpandConstant`. Both are i386
  emulation follow-ups ([evidence](../evidence/2026-10-12-gog-support-setup.md)); the record keeps
  this decision's part, which needs no change when they are fixed.

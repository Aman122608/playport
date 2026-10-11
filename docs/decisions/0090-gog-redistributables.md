# 0090: GOG's redistributables are fetched and run as GOG Galaxy and Heroic do

**Status:** accepted, 2026-10-11, by the compatibility speedrun's worker with the owner away (the
supervisor's direction "a store's own redistributables are allowed", as the owner allowed Steam's in
[0081](0081-steam-common-redistributables.md); the conservative option at each choice, recorded for
review); [PLA-182](https://linear.app/playportdev/issue/PLA-182),
[PLA-183](https://linear.app/playportdev/issue/PLA-183). Amends
[0079](0079-prerequisites-from-play.md)'s "only files the title ships" and 0081's "Epic and GOG
titles keep 0079's rule", for GOG titles.
[Evidence](../evidence/2026-10-11-openal.md).

## Context

Baldur's Gate: Enhanced Edition (GOG 1207666353) exits at once with `0xc0000135`: `Baldur.exe`
imports `OpenAL32.dll`. Neither Wine nor Proton has one (the wine pin's `dlls/` has no `openal32`;
Proton 10, 11 and Experimental ship none). On Windows and under Proton the DLL comes from the
store: the game's build manifest names `"dependencies": ["openAL"]`, GOG Galaxy installs that from
GOG's dependency repository, and Heroic, which runs GOG games under Proton, downloads the same
repository entry and runs its installer in the prefix (`__redist/openAL/oalinst.exe /S`,
Creative's OpenAL 2.0.7.0 installer: `OpenAL32.dll` and `wrap_oal.dll` into `system32` and
`syswow64`). GOG's repository lists every redistributable a build may name: VC++ 2005 to 2026, the
.NET Framework, .NET Core and Desktop Runtimes, DirectX June 2010, PhysX, XNA, OpenAL, UE4's
prerequisites, and tools GOG places for the game itself (its script interpreter, DOSBox, ScummVM).
Its depots need no sign-in.

## Decision

- **Which.** For a GOG game, the ids its build manifest names in `dependencies` (kept on the install
  record; an older record reads them from its build once), looked up in GOG's dependency repository
  (`content-system.gog.com/dependencies/repository?generation=2`, kept in
  `manifests/gog-dependencies.json` and fetched again when it lacks an id). Nothing for an id the
  build does not name, nothing bundled.
- **Not run, recorded as skipped:** `DirectX` (its DXSETUP, as Steam's is skipped: decision
  [0086](0086-directx-setup-skipped.md)) and the .NET Framework installers (`dotNet35` to
  `dotNet472`: Wine Mono stands in, as the prefix's .NET HasRunKeys make Steam skip its own .NET
  entries under 0081). .NET Core and Desktop Runtimes are not the Framework and run.
- **Left out, not recorded (logged):** an id GOG does not list, a tool with no installer (Heroic
  skips it too), and an installer that is not an `.exe` (XNA's `.msi`), which the prerequisite path
  cannot start.
- **Where the files go.** Into the game's own folder, at the depot's paths (`__redist/<id>/…`, as
  GOG's offline installers place `__redist`), downloaded at the first Play that needs one (the Play
  says "Downloading GOG redistributables…") through the GOG client's content engine, each chunk
  md5-checked, from GOG's open link. An installer already in the folder is not fetched again; the
  game's uninstall removes them. No session or a failed download leaves that installer out and the
  game starts.
- **How it runs.** Through 0079's mechanism with 0083's and 0084's rules: id `gog-redist:<id>`, once
  per prefix, a failure once more per app build, the restart before the game when one ran. The
  arguments are the repository's (`/S` for OpenAL), as Heroic passes them. No registry is written
  after a success: GOG's repository names none.
- **Not covered.** A build's `scriptInterpreter` (GOG's install script, `goggame-<id>.script`, run by
  `__redist/ISI/scriptinterpreter.exe`) and v1 builds' `temp_executable`: Heroic runs those too; they
  are a follow-up.

## Why

It is what GOG's own client does on Windows and what Heroic does under Proton, for the same game, from
the store's own distribution channel: the parity rule (a game that works under Proton without
protontricks works here). Shipping a library instead (OpenAL Soft's `openal32.dll`, LGPL) would need
the owner's licensing decision and would differ from what the store installs. Placing the files in the
game's folder needs no new shared folder, no library exclusion and no Storage row: they go with the
game.

## Consequences

- Baldur's Gate EE's first Play downloads 0.8 MB and runs `oalinst.exe /S`, then restarts (0084).
- A GOG game that names VC++ downloads 14–26 MB per installer into its own folder; two games naming
  the same id each hold a copy (Steam's are shared). The installer runs once per prefix by id.
- The repository's signature fields are not checked; the transport is HTTPS and each chunk is
  md5-checked against the depot manifest.

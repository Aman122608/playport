# 0081: Steam's Common Redistributables are fetched as Steam does, for a title that lists them

**Status:** accepted, 2026-10-11, by the owner: "Playport may download the Steamworks Common
Redistributables (app 228980) depot *as Steam does*, for a game whose `installscript.vdf` lists it,
and run the listed installer once per prefix" (relayed by the supervisor of the PLA-163 session);
[PLA-158](https://linear.app/playportdev/issue/PLA-158). Amends
[0079](0079-prerequisites-from-play.md)'s "Steam's installscripts are not read yet" and the rule that
Playport never downloads Microsoft's redistributables, for this case only. Two implementation
notes below (no retry, not shown) are amended by [0083](0083-prerequisite-rerun-per-build.md);
"Epic and GOG titles keep 0079's rule" is amended for GOG by [0090](0090-gog-redistributables.md).

## Context

A Steam game's `installscript.vdf` names the prerequisites Steam runs once before the first launch
(`Run Process`: `process 1`, `command 1`, a `HasRunKey` Steam writes after a success). Most point into
`_CommonRedist`, which Steam fills from the shared depots of app 228980 (Steamworks Common
Redistributables: DirectX June 2010, VC++ 2008–2022, .NET, OpenAL, PhysX), not from the game's own
depots. Playport's Steam client downloads only a game's own depots, so those installers are absent
and the game misses `mfc140u`, `openal32`, `xinput1_3` variants or a newer VC++ build.

## Decision

- **Which.** For a Steam game whose `installscript.vdf` lists 228980 content, Playport downloads the
  228980 depot(s) the script needs, through the same Steam client and account, as Steam does; a
  title's own `_CommonRedist` files are used as they are.
- **How it runs.** Each listed `Run Process` entry runs through 0079's prerequisite mechanism, from
  Play, once per prefix, with the entry's `HasRunKey` as its id (and written to the prefix's registry
  after a success, as Steam writes it). 0079's rules hold: failure and timeout do not block, the
  launch screen says so, the record is the prefix's `playport-prerequisites.json`.
- **Nothing else.** No redistributable is bundled in the app, none is downloaded for a title whose
  script does not list it, and Epic and GOG titles keep 0079's rule (only what the title ships).

## Why

It is what Steam itself does for the same game on Windows and under Proton, so the installer a title
asks for is the one it gets, from the publisher's own distribution channel.

## Consequences

- Implementation is PLA-158, not part of PLA-163's change: installscript parsing for the game's own
  depots and the 228980 entries, the depot selection, the prerequisite ids.
- The first Play of such a title downloads and runs the redistributables before the game.

## Implementation notes (PLA-158, 2026-10-11)

What the implementation settled where this record is silent, the conservative choice at each
(owner away; [evidence](../evidence/2026-10-11-steam-install-scripts.md)):

- **Which depots.** The 228980 depots the game's app info lists as `depotfromapp` 228980 (its install
  record keeps them as skipped redistributables), as Steam installs them; each depot's own install
  script (the file its manifest flags) names what runs. They are downloaded at the first Play that
  needs one (the Play button says so), as one install of app 228980 into `Games/Steamworks Shared`
  holding every depot any game needed; the library does not list it. No session or a failed download
  leaves those entries out and the game starts.
- **When an entry applies.** As Steam on 64-bit Windows 10: `Is64BitWindows` 1 holds (so x86 and x64
  entries both run, as Steam runs both), `OSType` holds only for Windows 10, an unknown requirement
  does not hold. Entries outside `%INSTALLDIR%` or whose file is absent are left out (logged).
- **Has it run.** 0079's record, or Steam's own check: the prefix's `system.reg` holds the HasRunKey
  value at least `MinimumHasRunValue`. The seed carries the HasRunKey values Proton's `wine.inf` writes
  (.NET 3.5–4.8, XNA 4.0), so Microsoft's .NET installers are not run over Wine Mono, as under Proton.
  A failure is recorded once as 0079 says; it is not retried at the next Play (Steam retries).
- **Writing HasRunKey.** After exit 0 or 3010, the value (entry name = 1, Steam's form; one registry
  view, decision 0072) goes into a file beside the hives that the runtime merges at its next start,
  as it merges the seed; the session root gets no registry request. The scripts' `Registry` values
  are written the same way before the game's runtime starts.

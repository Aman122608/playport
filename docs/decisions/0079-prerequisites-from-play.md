# 0079: A title's own prerequisite installer runs once, from Play

**Status:** accepted, 2026-10-11, by the compatibility speedrun's worker with the owner away and
the supervisor's choices (the conservative option at each, recorded for review);
[PLA-113](https://linear.app/playportdev/issue/PLA-113),
[PLA-67](https://linear.app/playportdev/issue/PLA-67).
[Evidence](../evidence/2026-10-11-prerequisites-from-play.md). Amended by
[0081](0081-steam-common-redistributables.md) (Steam's install scripts) and
[0083](0083-prerequisite-rerun-per-build.md) (a failure runs once more with each new app build) and
[0084](0084-prerequisites-before-a-restart.md) (after a prerequisite ran, Playport restarts and the game
starts in the new process) and [0090](0090-gog-redistributables.md) (a GOG game's redistributables,
from GOG's dependency repository) and [0095](0095-launch-screen-dependencies.md) (the launch screen's text and step for the installs).

## Context

Some games need a runtime Wine does not have and Playport does not ship. House of Golf 2's
Razer Chroma plugin imports `mfc140u.dll` (MFC, from the VC++ 2015-22 redistributable); without
it the game calls a null pointer about 75 s in. On Windows the redistributable is on the machine
already. Steam runs a game's `_CommonRedist` installers once, Epic's launcher runs a build's
prerequisite installer (its manifest's `PrereqIds`, `PrereqPath`, `PrereqArgs`), and an Unreal
Engine 4 or 5 game's bootstrap runs `Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe` itself
when the VC++ runtime's registry key is missing. Under Proton the key comes from `wine.inf`,
and so it does in Playport's prefix seed (since PLA-67's b216243): the bootstrap never runs its
installer, and nothing else does. House of Golf 2's Epic manifest names no prerequisite. The
owner's rule stands: Playport never ships or downloads Microsoft's redistributables; it may run
the copy a title ships, on the phone, as Steam and Heroic do.

## Decision

- **Which.** Only files the title ships, in its own folder: the Epic build's prerequisite
  (manifest `PrereqPath` with `PrereqArgs`, for its `PrereqIds`, now kept on the manifest model
  and the install record; an older record gets it from the kept manifest once), and Unreal
  Engine's `Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe` with `/quiet /norestart` (the
  arguments that need no person), for a title of any store. When both name the same file it
  runs once, as the store's.
- **Once per prefix.** Each prerequisite id runs once in the prefix (the one shared prefix):
  Epic's ids as Epic names them, the UE installer's as `UEPrereqSetup_x64:<size>-<sha256
  prefix>`, so a newer installer runs again. The record is `playport-prerequisites.json` at the
  prefix's root, beside its registry hives, so a new prefix runs them again; it keeps each id's
  exit code, date and title. An unreadable record is an empty one.
- **Failure does not block.** A non-zero exit is recorded with its code and the game starts, as
  Epic's launcher continues. One still running after 300 s gets the in-game menu's close
  (`WM_CLOSE`, its job ended 5 s later) and is recorded with the code it ended with. One that
  cannot start (its file missing) is not recorded and the game starts. One the player ends with
  Quit game is not recorded, and the game does not start.
- **Path.** Part of Play, never its own entry point (decision 0012): after the runtime starts and
  before the game, the session root starts it as its child, in a job, with the game's
  environment, waits for it and its children, flushes the registry and takes the next request
  (a new `PP_REQUEST_PREREQUISITE`, wine_host ABI 6 `wine_host_session_prerequisite`). Its
  windows show as a game's launcher's do (decision 0075). The launch screen says "Installing
  <title>'s prerequisites…" (a launch stage of its own); the `title:` log has `prerequisite:`
  lines and `prerequisite started` / `prerequisite ended` marks, a dev build a `prerequisite`
  event.
- **Steam's installscripts are not read yet.** A Steam game's `installscript.vdf` names its
  `_CommonRedist` installers and the registry key Steam writes after each; most point into the
  shared redistributable depots (app 228980), which Playport does not download (the same rule).
  The UE installer is covered for Steam games too. A follow-up issue tracks the rest.

## Why

Steam, Epic's launcher and Heroic run a title's own prerequisites once at install or first
launch; Proton relies on that. Running the same file the bootstrap would run on Windows, once,
is the smallest step to parity that ships nothing. Dropping the seed's VC++ key instead would
make every UE bootstrap ask the player first and lose parity with Proton's prefix. Running from
Play, not at install, keeps the one prefix the place where the runtime lives (the record goes
with it) and needs no runtime outside a Play (decision 0029's one session per process).

## Consequences

- The first Play of such a title takes longer by the installer's run (House of Golf 2's: under
  a second when its packages are already present, about 6 s when it installs them).
- An installer that needs a person (no quiet switch) shows its window over the launch screen;
  if it never ends, the 300 s limit ends it.
- The prerequisite and the game share a session, so the runtime's per-session state must not
  carry one guest process's settings into the next: the first run found win32u's `zero_bits`
  set by the i386 installer for the x86-64 game (fixed by wine-unix 0027).

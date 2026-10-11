# 0083: A failed prerequisite runs once more with each new app build

**Status:** accepted, 2026-10-11, by the supervisor of the compatibility speedrun with the owner
away (the conservative option at each choice, recorded for review);
[PLA-171](https://linear.app/playportdev/issue/PLA-171). Amends
[0079](0079-prerequisites-from-play.md)'s "once per prefix" for failures and two of
[0081](0081-steam-common-redistributables.md)'s implementation notes: "it is not retried at the
next Play" and "the library does not list it".
[Evidence](../evidence/2026-10-11-system32-programs.md).

## Context

0079 runs each prerequisite once per prefix and records the result whatever the exit code. A
failure caused by the runtime therefore stayed recorded after the runtime was fixed. On the
phone this happened to Counter-Strike's VC++ 2019 entries, which failed for want of
`start.exe` (fixed by [0082](0082-system32-programs.md)), and to Castle Crashers Demo's
DXSETUP. Steam itself retries an entry until its HasRunKey is written. Retrying at every Play
would repeat a failure the runtime cannot change, for example a downgrade refusal, at the
cost of time and JIT pool each Play.

The 228980 depots also had no place in the UI. They live in `Games/Steamworks Shared`, which
the library does not list, so a player could not see or free that space.

## Decision

- **A recorded failure runs once more when the app build differs from the build that recorded
  it.** The new run is recorded with the new build, so the next Play on that build skips it.
  - A success (exit 0 or 3010, `Prerequisite.successCodes`) never runs again.
  - A record written before builds were kept (no `build`) counts as another build, so the
    failures already on a phone run once more.
  - Only 0 and 3010 are successes. 1638, "a newer version is installed", is a failure as
    Steam's own scripts treat it: an older VC++ redistributable correctly refuses to downgrade
    a newer one in the one shared prefix. It runs once per build, a few seconds.
- **The app build** is the first 16 hex digits of the SHA-256 of two things: the runtime's
  record (`artifacts.tsv`: every staged file's digest) and the main executable's LC_UUID (the
  linker's digest of the app's own code). It is the same for every install of one IPA, and
  different after a build that changed a runtime file or the app's code. A change only to
  docs or tools makes the same build.
  - `playport-prerequisites.json` keeps it per run (`build`).
  - The `title:` log names it on a rerun ("failed with another build …; build X runs it
    again") and on each record.
- **Settings › Storage shows `Steamworks Shared`** with its size from its install record, after
  the games, when it is installed. A removes it as an uninstall does: the folder, the record
  and the manifests. Not while a game runs or a Play is preparing. The next Play of a game
  that lists those depots downloads them again (0081). The library still does not list it.

## Why

A new build is the moment a runtime fix can turn a failure into a success, and the moment the
player expects a new attempt. Between builds a rerun cannot succeed where the last one failed.
Building the identity from the bundle's own record and the binary's UUID needs no build-time
stamping and no new launch path.

## Consequences

- The first Play after an update reruns that title's recorded failures (seconds each, the
  prerequisites' child processes in the pool: [0082](0082-system32-programs.md)'s costs).
- An entry that can never succeed in this prefix (DXSETUP's Managed DirectX check today, or a
  VC++ downgrade) reruns once per build.
- Removing `Steamworks Shared` costs a re-download at the next such Play (143 MB in 6 s for
  VC++ 2022 and its companion depot on the phone).

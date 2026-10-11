# 0084: A Play whose prerequisites ran restarts Playport before the game

**Status:** accepted, 2026-10-11, by the compatibility speedrun's worker with the owner away (the
conservative option, recorded for review);
[PLA-172](https://linear.app/playportdev/issue/PLA-172). Amends
[0079](0079-prerequisites-from-play.md)'s "Path" (the game no longer starts in the session its
prerequisites ran in) and [0029](0029-restart-after-each-game.md)/[0030](0030-one-title-per-process.md)
(a Play can span two processes: the restart carries it).
[Evidence](../evidence/2026-10-11-prerequisites-before-a-restart.md).

## Context

0079 runs a title's prerequisites as children of the game's session root, in the same app
process, before the game. Since system32 has `start` and `conhost`
([0082](0082-system32-programs.md)), a Steam VC++ entry is a real process chain: `cmd`,
`conhost`, `start`, the installer and its own children. Each of those pseudo-processes takes
its own image copies and ntdll copy from the JIT pool's head, and the head returns nothing:
a dead process's image ranges are dropped, not reused (docs/ARCHITECTURE.md, "JIT pool use").
Counter-Strike's VC++ 2019 pair left the head at 225 to 232 of 512 MiB before the game started;
without them it is 20 at the game's start. Under [0083](0083-prerequisite-rerun-per-build.md)
a failure runs again with each new build, so a heavy game could run out of JIT memory on its
first Play, or on the first Play after an update.

Three ways were open:

- **Reclaim the pool when a process exits.** The head's image ranges are dropped on purpose;
  reusing freed pool ranges has a history of poisoned pages (`virtual_ios.c`'s reuse notes).
  A runtime patch here needs its own investigation and a play gate, and would change the pool
  for every launcher, not only prerequisites.
- **Run prerequisites in a session of their own, then the game in a fresh one.** One app process
  runs one Wine session and one blessed pool (0029, 0030), so a fresh session is a fresh process.
- **End the session after any prerequisite ran and continue the Play in a new process.** The
  same as the previous one, using the restart Playport already makes after every game.

## Decision

- **When at least one prerequisite ran** (it started and ended, whatever its exit code, the 300 s
  close included) **and Playport can restart itself**, the launch ends after the prerequisites
  instead of starting the game. Its outcome is its own (`launch=restart prerequisites=N`, logged,
  not a `title: done` line): no alert, no play time.
- **Playport restarts** through `AppRestart`, as after a game. The restart record
  (`Documents/restart.json`) carries the Play (PlayportKit `PlayContinuation`: the title id, its
  name, its Steam app, Epic's play-offline choice).
- **The new process continues the same Play.** Once its library is scanned and the opening is
  done, it calls the library's own Play for that title, with no cloud save check and no
  prerequisites, so it cannot restart a second time. The game gets a fresh process, session and
  JIT pool.
- **The screen** shows the launch screen's art, the game's name and "Starting <title>…" from the
  restart request until the new process's launch screen. iOS shows its Home Screen for a moment
  while the process is replaced, as after every game.
- **No restart possible** (inside LiveContainer, or JIT from another app with no pairing): the game
  starts in the same session after its prerequisites, as before. If the restart request fails,
  the existing "could not restart" alert shows; the prerequisites are recorded, so the next Play
  starts the game straight away.
- **Unchanged:** which prerequisites run and when (0079, 0081, 0083); a prerequisite that cannot
  start (not recorded, not counted); Quit game during a prerequisite (not recorded, no game; a
  Quit game after the last one also ends the Play); the 300 s limit and its close.
- **The pool is logged at the game's start** (`title: at the game's start: pool: …`), so a Play
  with prerequisites can be compared with one without.

## Why

The restart already exists, is proven on the phone after every game, and gives the game exactly
the process a Play without prerequisites gets. It changes no runtime code and no pool rule. The
cost falls only on a Play whose prerequisites actually ran: once per prefix, or once per build
for a failure.

## Consequences

- Such a Play takes about 9 s longer: the restart (0.4 s to the new process), the new process's
  library and store list, a second JIT acquire (about 2.5 s) and runtime start.
- The second JIT acquire needs LocalDevVPN, as every Play does.
- A dev build's driven run follows the Play across the restart (`Dev/DriverContinuation.swift`:
  the run's variables are handed on even with no actions left); `pp ui --until "mark:game started+N"`
  reaches the mark in the new process.
- A game the prerequisites' pool use would have broken now starts with a clean pool. Whether
  Counter-Strike's own host fault (PLA-164) was made worse by the shared session is not known.

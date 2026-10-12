# 0094: The restart after prerequisites only when they left the game too little pool

**Status:** accepted, 2026-10-12, by the compatibility speedrun's worker with the owner away (the
conservative option, recorded for review). The owner asked for it: he had not known about the
restart. [PLA-209](https://linear.app/playportdev/issue/PLA-209). Amends
[0084](0084-prerequisites-before-a-restart.md)'s "Decision" (the restart no longer follows every
Play whose prerequisites ran).
[Evidence](../evidence/2026-10-12-prereq-no-restart.md).

## Context

0084 restarts Playport between a title's prerequisites and its game whenever one ran, because each
of their processes leaves its image copies and ntdll copy in the JIT pool's head, which never
returns them. That costs about 9 s and a Home Screen flash, often for nothing. Measured from the
`[jit-pool]` lines of the phone's logs:

- **A Steam install-script VC++ entry** runs Steam's `.cmd` (`start /w "" "%~dp0\VC_redist.x86.exe"
  /q /norestart`, then 3010 mapped to 0). That makes five or six processes and takes about 100 MiB
  of head. `cmd` and `conhost` take 54 MiB together and `start` 30, since each copies ntdll, FEX's
  `libarm64ecfex.dll` (4.5 MiB), kernelbase and, for `conhost` and `start`, shell32 (9.7 MiB),
  user32 and the rest. The WiX Burn installer is i386, and each of its two or three processes takes
  only 7 MiB (ntdll, wow64, `libwow64fex`, wow64win, win32u).
- **A GOG redistributable** (0090) runs its installer directly. OpenAL's took 7 MiB.
- **DirectX** is skipped (0086) and takes nothing.

Three ways were open:

- **Return a dead process's image ranges to the head.** This is the proper fix. Those ranges lose
  execute permission before they are freed, and reusing VA with narrowed protection is the
  "poisoned pages" history in `virtual_ios.c` (29 of 29 poisoned ranges were reused). It needs a
  runtime investigation and a play gate of its own.
- **Make prerequisite processes cheaper.** Each pseudo-process needs its own image copies, at a
  fixed offset from its own data. Sharing them is a loader change of the same depth. Running the
  installer a `.cmd` names without `cmd` and `start` would save 84 of the 100 MiB. But Playport
  would then interpret batch files and depart from running what Steam runs (0081, 0082).
- **Restart only when the head is short.** Compare the room the game would get with what a heavy
  game needs, and skip the restart when there is enough.

## Decision

- **The restart follows a Play's prerequisites only when they left the game less than 320 MiB**
  (PlayportKit `JitPool.gameRoomMB`), when the pool ran out during them, or when the pool could not
  be read. The game's room (`JitPool.Use.roomForGame`) is the pool less the head's cursor and the
  tail's live code buffers: FEX reuses a dead process's code buffers, but never its image ranges.
- **320 MiB** is the heaviest high-water mark measured, The Witcher 3's head of 206 MiB plus its
  tail of 145, less the session root's own head (20 MiB) and tail (17), rounded up. Those marks
  predate madeira-unix 0090, which only lowered heads.
- **Otherwise the game starts in the same session**, as it already did when Playport cannot
  restart itself. The launch logs `title: prerequisites: N ran, room_for_game_mb=R (needs 320); …`
  either way, and `title: at the game's start: pool: …` before the game.
- **Unchanged from 0084:** the restart itself, its record, the continued Play and its screen; no
  restart inside LiveContainer or with JIT from another app; which prerequisites run and when
  (0079, 0081, 0083, 0090).

## Why

This change touches no runtime code and no pool rule, and it removes the restart where it
protected nothing. A GOG redistributable, or one Steam VC++ entry (376 MiB left for the game),
now goes straight to the game. A VC++ x86 and x64 pair in a fresh prefix (263 MiB left) still
restarts. So does a pair under 0083's rerun of a failure with each new build. The fallback is the
safe side: a Play that restarts when it did not need to costs 9 s, and one that skipped a restart
it needed could run out of JIT memory once.

## Consequences

- Most Plays with prerequisites start the game about 9 s sooner and show no Home Screen.
- A game that started after prerequisites in the same session has up to 155 MiB less pool than a
  fresh one. That is still 320 MiB, more than any game measured needs.
- A VC++ pair still restarts. Two follow-ups would end that: return dead image ranges to the head
  (the runtime fix), or run the installer of Steam's standard VC++ `.cmd` directly.
- A dev build forces the restart path with a smaller *Simulated JIT memory* pool
  (`jitPoolSimulatedMB`).

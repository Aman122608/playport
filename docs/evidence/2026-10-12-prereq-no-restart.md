# 2026-10-12: A Play whose prerequisites leave enough pool starts its game without a restart (PLA-209)

**Linear:** [PLA-209](https://linear.app/playportdev/issue/PLA-209) (follows
[PLA-172](https://linear.app/playportdev/issue/PLA-172)).
**Decision:** [0094](../decisions/0094-restart-only-when-the-pool-is-short.md), amending
[0084](../decisions/0084-prerequisites-before-a-restart.md).
**Questions:**
- How much JIT pool head does each kind of prerequisite take, and why?
- With the restart kept only for a short pool, does a title with first-run prerequisites reach its
  game in the same session?
- Does the restart still work when the pool is short?

**Result:**
- A Steam VC++ `.cmd` entry takes about 100 MiB of head. Of that, 84 MiB is `cmd`, `conhost` and
  `start`.
- An installer run directly takes 7 to 14 MiB.
- Counter-Strike's VC++ 2019 x86 entry ran and its game started 0.27 s later in the same session,
  with 377 MiB of room for the game.
- A forced short pool still restarts, and the new process continues the Play.

## Head per prerequisite

These figures come from the `[jit-pool] image …` and `[child-ntdll] copied …` lines of the phone's
logs. The head cursor is the `used=` field, attributed to the latest `NtCreateUserProcess` (the
parsing script is in the build area, `sweep08a/headuse.py`). Pseudo-processes overlap, so the
`cmd`/`conhost` split is approximate. Their sum is exact.

| Kind | Processes | Head | Run |
| --- | --- | --- | --- |
| Steam install-script VC++ 2019 x86 (`Microsoft Visual C++ 2019 x86.cmd`) | `cmd`, `conhost`, `start`, Burn `VC_redist.x86.exe`, its `.cr` clean-room copy | 99.2 MiB (19.5 → 118.7) | `20261012T085407` (and the same on `20261011T112614`) |
| Steam VC++ 2019 x64 `.cmd` | the same, with a third Burn process (`.be`) | 106 MiB (118.7 → 224.9) | `20261011T112614` |
| Epic `PrereqPath` VC++ x64 (Jurassic World Evolution, `Redistributables\VC_redist.x64.exe /quiet /norestart`) | Burn and its `.cr` copy | 14 MiB (20 → 34) | `20261012T085852` |
| GOG OpenAL (0090, `oalinst.exe`) | the installer | 6.9 MiB (19.5 → 26.4) | `20261011T221218` |
| Steam DirectX Jun2010 | skipped (0086) | 0 | none |

Here is what each process copies into the head:

- **An x86-64 (arm64ec) program** (`cmd`, `conhost`, `start`):
  - its own image (`cmd.exe` 1.5 MiB), its ntdll copy (1.25 MiB), FEX's `libarm64ecfex.dll`
    (4.5 MiB), `ucrtbase` (1.25), `kernel32` (0.8) and `kernelbase` (3.1). That makes 12.4 MiB
    before any other DLL.
  - `conhost` and `start` add `shell32` (9.7 MiB), `shlwapi`, `shcore`, `advapi32`, `sechost`,
    `msvcrt`, `user32` (2.2), `gdi32`, `win32u`, `imm32` and `cryptbase`. `conhost` also adds
    `uxtheme`, `ole32`, `combase`, `rpcrt4` and `coml2`.
  - `cmd` and `conhost` take 54 MiB together; `start` takes 30.
- **An i386 (WoW64) installer process** (the WiX Burn bundles VC++ ships, and GOG's OpenAL):
  ntdll, `wow64`, `libwow64fex`, `wow64win` and `win32u`. That is 6.8 MiB. Its i386 DLLs get no
  copy.
- Steam's `.cmd` is `start /w "" "%~dp0\VC_redist.x86.exe" /q /norestart`, then
  `IF %ERRORLEVEL% == 3010 EXIT /B 0`. The installer is the cheap part of the chain.
- **Not measured:**
  - Steam's .NET 4.8 `.cmd` would pay the same 84 MiB chain, plus its installer.
  - Wine Mono runs inside the game's process and has no prerequisite process.

**What a game needs** (decision 0036's high-water marks, measured before madeira-unix 0090
lowered heads):
- Hollow Knight: head 187 MiB, tail 145 MiB.
- The Witcher 3: head 206 MiB, tail 145 MiB.
- Kingdom Come: Deliverance: head 182 MiB, tail 161 MiB.

**On this IPA:**
- Hollow Knight's menu: head 83 MiB, tail 161 MiB.
- Portal 2's splash: head 27 MiB, tail 161 MiB.
- Each game's 128 MiB FEX code buffer came from the room between head and tail. On Counter-Strike
  after its prerequisite, the tail cursor went from 97 to 225 MiB. Its live buffers went from 17 to
  161 MiB: 128 MiB from the room, plus one reused 16 MiB buffer.

## Phone

All runs are on the iPhone18,4 with iOS 27.0, using the dev IPA
`c8e5ffaa98e69806a8fee4667a3242c9092c62651ca537b4608995f0ec2b3918`, built from a096230 (the
change; this record's commit only adds documentation and a comment). Plays use
`pp ui --wait 180`. Run directories are under `.work/ui-runs/`.

### Counter-Strike (`app-10`), no restart: `20261012T085407`

The prefix's record had VC++ 2019 x86 failing (0x666) under another build, and x64 succeeding.
The new build therefore ran x86 again (decision 0083). This is the state a fresh prerequisite
gets through the UI: nothing in the UI resets the record.

```
title: +5.00 s prerequisite started
session: prerequisite 1: ended, exit code 666 after 1348 ms
title: +6.69 s prerequisite ended
title: prerequisites: 1 ran, room_for_game_mb=377 (needs 320); the game starts in this session (decision 0094)
title: at the game's start: pool: size_mb=512 head_mb=119 head_live_mb=20 head_free_mb=100 tail_mb=97 tail_live_mb=17 room_mb=297 … exhausted=none
title: +6.96 s game started
title: +7.38 s first frame
```

- There was no restart and no Home Screen. The game started 0.27 s after the prerequisite ended,
  where 0084's restart took 9.2 s.
- At `mark:game started+20` the pool read `head_mb=126 tail_mb=225 tail_live_mb=161 room_mb=162
  exhausted=none`.
- The screenshot (`screen-stop.png`) shows Counter-Strike's main menu (New game, Find servers,
  Options, Quit).

### The restart when the pool is short (forced): `20261012T085852`

- **Set-up:** Jurassic World Evolution (`epic-373a4372c20540aca7fdd880d27fa49a`), with its Epic
  VC++ x64 entry pending again (0x666 by another build). The driver set `set:jitPoolSimulatedMB=352`
  for the session.
- **Old process:** the entry ran (exit 0x666, 0 s). Then:
  - `prerequisites: 1 ran, room_for_game_mb=302 (needs 320); Playport restarts and the game starts in the new process`
  - `launch=restart prerequisites=1`
- **Restart:** `restart: new process pid 39270, 521 ms after pid 39264 asked; continuing the Play of Jurassic World Evolution`.
- **New process:**
  - `continued after its prerequisites ran before the restart; none run now`
  - `title: at the game's start: pool: size_mb=352 head_mb=20 … room_mb=316`
  - The game started at +3.15 s.
- **The game:** it then ended with `0xc0000005` after 7.8 s, before a first frame (an unhandled
  exception at `0x1488a3cf6`, `crashes/S1Probe-2026-10-12-020218.ips`), and the pool did not run
  out (`head_mb=83 room_mb=109 exhausted=none`). The game is listed as Broken in the compatibility
  list. This run shows that the restart path works. It says nothing about the game.

### Hollow Knight with 320 MiB of room from its start: `20261012T085733`

`set:jitPoolSimulatedMB=357` gives a fresh process exactly the room the rule guarantees: 357 MiB
less the session root's 20 MiB of head and 17 MiB of tail is 320.

- The first frame came at +9.02 s, and the menu showed at +30 s.
- The pool read `size_mb=357 head_mb=83 tail_mb=161 room_mb=114 exhausted=none`. FEX still got its
  128 MiB buffer.

### Gates

| Title | Run | First frame | Screen |
| --- | --- | --- | --- |
| Hollow Knight (`app-367520`, `--until first-frame+10 --shot`) | `20261012T085531` | +9.55 s | main menu (Start Game, Options, Achievements, Extras, Quit Game); pool head 83, tail 161, room 269 |
| Portal 2 (`app-620`, `--until first-frame+10 --shot`) | `20261012T085628` | +6.24 s | the Source splash; pool head 27, tail 161, room 325 |

No runtime or pool code changed, so there was no `pp perf` run.

## Host checks

- PlayportKit:
  - `PrerequisitesTests.testAPlayRestartsBeforeItsGameOnlyAfterAPrerequisiteRan`
  - `PrerequisitesTests.testAPlayRestartsBeforeItsGameOnlyWhenThePrerequisitesLeftTooLittleRoom`
    (the measured pools: one entry 376 MiB, no restart; the pair 263 MiB, restart; OpenAL; the
    bound; a pool that ran out; a pool that could not be read).
- `pp check` compiles the dev and the release variants. `pp test --quick` passes.

## Not checked

- A fresh x86+x64 VC++ pair on this IPA. Its restart follows from the same rule (263 MiB < 320),
  and the forced run above exercises the path.
- Inside LiveContainer, or JIT from another app. Nothing changed there: the game always starts in
  the same session.
- A heavy game's whole session (The Witcher 3, Kingdom Come) after prerequisites in the same
  session. The bound uses their high-water marks from before 0090.
- The release variant on the phone. Owner sign-off applies (decision 0068).

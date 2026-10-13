# 2026-10-12: The launch screen names dependency installs, with their own step (PLA-208)

**Linear:** [PLA-208](https://linear.app/playportdev/issue/PLA-208).
**Decision:** [0095](../decisions/0095-launch-screen-dependencies.md).
**Questions:**
- Which first-run dependency work runs before a game's first frame, and what did the launch screen
  say during it?
- Does a Play that installs dependencies show "Installing dependencies…" and a fourth step dot, and
  one that installs none keep three?
- Does the restart screen say the dependencies are done?

**Result:**
- Only a title's prerequisite installs (Steam install scripts with the 228980 depots, Epic
  `PrereqPath`, Unreal Engine's installer, GOG redistributables) run during the launch screen. A
  Steam `.cmd` entry's console window turned the launch's stage into the game's ("Starting the
  game…") about 0.4 s into the install.
- On the new IPA, Counter-Strike's VC++ 2019 x86 rerun shows "Installing dependencies…" with four
  dots (JIT and Runtime done, Dependencies and Game to come), and the stage stays there through the
  console window until the game starts.
- The forced restart (Jurassic World Evolution, short simulated pool) shows "Dependencies installed.
  Starting Jurassic World Evolution…".
- Hollow Knight and Portal 2 keep three dots and never show the dependency text; both reach their
  first frame.

## First-run work before the first frame

From the code and the `title:` logs of the compatibility runs under `.work/ui-runs/`:

| Kind | When | Launch screen before | After |
| --- | --- | --- | --- |
| Steam install-script entries (VC++ `.cmd`, .NET), 228980 depots (0081) | prerequisite stage; download before Play's screen | "Installing <title>'s prerequisites…", then "Starting the game…" from the console window (`first window` 0.4 to 0.5 s after `prerequisite started` in every `.cmd` run, e.g. `20261011T113752`, `20261012T085407`) | "Installing dependencies (n of m)…", Dependencies dot, stage kept through the window |
| Epic `PrereqPath` (JWE's VC++ x64 Burn bundle), UE `UEPrereqSetup_x64` (0079) | prerequisite stage; no window seen | "Installing <title>'s prerequisites…" | "Installing dependencies…", dot |
| GOG redistributables (0090, OpenAL) | prerequisite stage; download before Play's screen | as Epic | as Epic |
| DXSETUP (0086), GOG DirectX and .NET Framework (0090) | skipped before the runtime starts | nothing runs | unchanged |
| Wine Mono (0077) | no install step: `mscoree` finds the shipped release | none | unchanged |
| .NET Framework links (0085), registry seed and top-up, system32 links | the runtime's start ("Starting Windows…") | `registry system.reg: 5 of 8080 seed keys added` and `user.reg` within 2 ms; `runtime started` about 0.5 s after JIT arrives (this record's runs) | unchanged (no text needed) |
| 228980 and GOG downloads | before the launch screen | the game page's Play button: "Downloading Steam redistributables…" / "Downloading GOG redistributables…" | unchanged |
| Restart after a short pool (0084, 0094) | between the installs and the game | "Starting <title>…" | "Dependencies installed. Starting <title>…" |

No run in the build area shows a first-run prefix creation (the phone's prefix is long-lived); the
code (`wine_host.c`: `seed_profile`, `seed_etc`, `seed_registry`, `link_system32`) is file and link
work of the same kind as the top-up, so no "Setting up Windows" text was added.

## Phone

All runs: iPhone18,4, iOS 27.0, dev IPA
`1ebae0871ad815b741b21942657acb7f552b20d7d7e4cde4ff1f4cc659ab90e4`, built from 239283e (the
change; app build `32c31bea035df32e`). `pp ui --wait 180`. Run directories are under
`.work/ui-runs/`. The dev build's new `launch-screen` events give the screen's status and steps at
each change; the screenshots lag the events by about one to two seconds.

### Counter-Strike (`app-10`): `20261012T091403`

The x86 VC++ 2019 entry, failed (0x666) under build `bbb02bfcdbdfa350`, ran again under the new
build (decision 0083), the fresh install a player gets through the UI.

```
title: +5.23 s prerequisite started
title: +5.71 s first window
title: +6.81 s prerequisite ended
title: prerequisites: 1 ran, room_for_game_mb=377 (needs 320); the game starts in this session (decision 0094)
title: +7.08 s game started
title: +7.49 s first frame
```

Launch-screen events: "Waiting for JIT…" (JIT,Runtime,Game), "Starting Windows…", "Installing
dependencies…" (JIT,Runtime,Dependencies,Game) at `prerequisite started`, no change at `first
window`, "Starting the game…" at `game started` (four steps kept).

- `screen-0009s.png`: the launch screen over Counter-Strike's art, the bar a little under half,
  four dots (two filled, two empty) and "Installing dependencies…".
- `screen-0008s.png`: the same screen at "Waiting for JIT…" with three dots.
- `screen-stop.png`: Counter-Strike's main menu (New game, Find servers, Options, Quit).

The entry exited 0x666 again, as before; one install, so no count.

### Restart wording, forced: Jurassic World Evolution (`epic-373a4372c20540aca7fdd880d27fa49a`), `20261012T091522`

`--action set:jitPoolSimulatedMB=352` for the session, as in the 0094 evidence. Its Epic VC++ x64
entry (0x666 under the old build) ran again (0x666, 0 s), left 302 MiB, and Playport restarted.

- Events: "Installing dependencies…" (four steps) at +2.75 s; the restart; then, in the new process,
  "Waiting for JIT…", "Starting Windows…", "Starting the game…" with three steps (it installs
  nothing).
- `screen-0009s.png`: the restart screen: the game's art, its name and "Dependencies installed.
  Starting Jurassic World Evolution…".
- The game then exited `0xc0000005` after 7 s before a first frame, as in the 0094 evidence (listed
  Broken); this run says nothing about the game.

### Gates

| Title | Run | First frame | Screen |
| --- | --- | --- | --- |
| Hollow Knight (`app-367520`, `--until first-frame+10 --shot`) | `20261012T091635` | +9.61 s | main menu (Start Game, Options, Achievements, Extras, Quit Game); events: three steps, no dependency text |
| Portal 2 (`app-620`, `--until first-frame+10 --shot`) | `20261012T091738` | +6.41 s | the "powered by Source" splash; three steps, no dependency text |

## Host checks

- PlayportKit `LaunchProgressTests`: the bar's share per install and that it stays below the game's
  start; the status with and without a count and its seconds past 10 s; the restart wording; the
  steps with and without dependencies; `failedStage` for `wait=… prerequisite=…`.
- `pp check` compiles the dev and the release variants; `pp test --quick` passes.

## Not checked

- "(1 of 2)" and the seconds on the phone: no title here has two pending installs or a long one on
  this build (host tests only).
- Whether the console window ended the launch screen during the Counter-Strike install: the window
  came 0.48 s into a 1.6 s install, and the screenshots lag by more. The code ends the screen at the
  first guest window (0075), unchanged here.
- The failure view's Dependencies step on the phone (host test of `failedStage` only).
- The release variant on the phone (decision 0068).

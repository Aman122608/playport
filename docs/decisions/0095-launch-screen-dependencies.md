# 0095: The launch screen names dependency installs, with their own step

**Status:** accepted, 2026-10-12, by the compatibility speedrun's worker with the owner away (the
conservative option at each choice, recorded for review). The owner asked for it: on a first Play
the launch screen seemed stuck while dependencies installed.
[PLA-208](https://linear.app/playportdev/issue/PLA-208). Amends the launch screen of
[0079](0079-prerequisites-from-play.md) ("Installing <title>'s prerequisites…") and the restart
screen of [0084](0084-prerequisites-before-a-restart.md) ("Starting <title>…"), as
[0094](0094-restart-only-when-the-pool-is-short.md) left it.
[Evidence](../evidence/2026-10-12-launch-screen-dependencies.md).

## Context

The launch screen (PlayportKit `LaunchProgress`, `Launch.dc.html`) has three step dots, JIT,
runtime and game, and one line under its bar. A title's prerequisites (0079: Epic `PrereqPath`,
Unreal Engine's installer, Steam's install scripts with the 228980 depots of 0081, GOG's
redistributables of 0090) had a stage of their own, "Installing <title>'s prerequisites…", with
no dot. Three things could make a first Play look stuck:

- **A Steam `.cmd` entry's console window** (`conhost`) is the launch's first guest window
  (decision 0075). The app read that window as the game's (`.window` maps to the game stage), so
  from about 0.4 s into the first install the launch's stage was the game's, "Starting the
  game…", for the rest of the installs; a later install set it back without its dot.
- **The restart** between the installs and the game (0084, now only for a short pool, 0094) said
  "Starting <title>…" across a Home Screen flash, as if the game had started once already.
- **The bar** gave all installs one tenth of its width, easing over 30 s, and showed nothing for an
  install that ran on toward the 300 s limit.

Every other first-run kind was checked: DXSETUP and GOG's DirectX and .NET Framework entries are
skipped (0086, 0090) and run nothing; Wine Mono has no install step (0077); the .NET Framework
links (0085), the registry seed and its top-up, and the system32 links run at the runtime's start,
in milliseconds on the phone's logs; the 228980 and GOG downloads happen before the launch screen,
on the game page's Play button ("Downloading Steam redistributables…"). None needs a text of its
own.

## Decision

- **The text** is "Installing dependencies…", the owner's word, and "Installing dependencies
  (1 of 2)…" when the Play runs more than one. The count is those the prefix still needs, taken
  before the first runs (LaunchCoordinator, `LaunchPart`). An install running 10 s or more adds
  its seconds ("… 42 s"), so a long one visibly ticks. The installer's own name is not shown:
  Steam's entries carry only a script line ("x86 14.28.29334.0"), and a readable name would need a
  table per store.
- **A Dependencies dot** between Runtime and Game, from the first install of the launch on, and
  "Dependencies" in the listed steps. A Play that installs nothing keeps three dots. The screen
  does not know in advance: the plan's prerequisite list includes every entry that already ran, so
  the dot would show on every Play of such a title.
- **A window during an install is the installer's**: the stage stays on the dependencies until the
  game starts. Whether that window ends the launch screen is unchanged (0075).
- **The bar** gives each install an equal share of the dependencies' span (0.5 to 0.6), easing over
  30 s each, so each install moves it and none passes the game's start.
- **The restart screen** says "Dependencies installed. Starting <title>…".
- **A failure** while waiting on an install (`launch=failed wait=… prerequisite=…`) is the
  dependencies' step, not the game's; the failure view lists the Dependencies step when the launch
  installed any or failed at it.
- Both apps show the same text. A dev build emits a `launch-screen` event (the status and the
  steps) whenever the stage or the install changes, for the drivers' timeline.

## Why

The owner's report was the symptom; the stage mapping was the cause, and the count, the seconds and
the restart wording answer "is it stuck?" without a new screen. Showing the dot only once an install
starts never shows a step that does not run.

## Consequences

- A Steam `.cmd` install shows its console as a guest window (0075), which may end the launch
  screen before the game starts; after the console closes the screen is the game's black surface
  until its first frame. Keeping the launch screen up over an installer's window would need a
  decision on 0075.
- The continued Play after a restart has three dots: its process installs nothing.

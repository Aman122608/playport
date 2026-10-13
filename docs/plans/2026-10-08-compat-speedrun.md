# Plan: compatibility speedrun

**Date:** 2026-10-08 (started). **Kind:** execution plan; what remains is the owner's library
A–Z (below). **Linear:** [PLA-112](https://linear.app/playportdev/issue/PLA-112) (umbrella; each game has its own Compatibility issue, each
cross-title fault its own issue).

**Branch:** none open. Both legs are merged into `main` (`dev/compat-speedrun`, then
`dev/pla-152-gdi-windows` on 2026-10-13). Start the library run on a new branch from `main`.

## How to pick this up in a new session

1. Read this file and Linear PLA-112.
2. The supervisor runs it with the plan-runner skill (`.pi/skills/plan-runner`): ask the owner
   for the hard/easy models, one writer at a time on a branch from `main`, the per-game brief below.
3. Check `git status` (clean) and **Running now** below: if the named child's commit is in
   `git log`, its chunk finished (read its report in `.work/agent-notes/speedrun/reports/`, write
   Linear from it, mark it done here); if not, it was cut off: re-launch that chunk from the
   current HEAD. Then take the first item of **Queue**.
4. Phone: `./pp phone status` (free space ≥ 5 GB for installs; see Phone state below).

## Goal

As many titles playable as possible: first the Linear Compatibility issues we can test, then
the owner's library alphabetically. **Proton parity is the target** (owner, 2026-10-11): if a
title runs under Proton without protontricks, it should run here; a difference is our bug.

## Settled (owner, 2026-10-08 to 2026-10-11)

- One quick worker per game, run by the supervisor in the plan-runner manner. Models chosen
  per session (2026-10-10/11: hard = Opus 5.5 medium, easy = Sonnet 5.5 medium).
- Vulkan only (DXVK/vkd3d-proton; decision 0100 rules DXMT out); a Vulkan failure is filed as a bug.
- Every play at 16:9 (decision 0073). A setting a title needs by default goes into
  `app/PlayportKit/Titles/compat-defaults.json` (decision 0074).
- Done = first frame + 30 s with something visible on a screenshot; write the game's Linear
  issue and `site/compatibility/games.json`; next game.
- No Hollow Knight gate per game. Exception: a change to window creation, the prefix, the
  launch path or the fault path gets a quick Hollow Knight (`app-367520`) and Portal 2
  (`app-620`) first-frame+10.
- Don't assume unplayable (launchers, online, store sign-in). Skip only anti-cheat that blocks
  Linux. OpenGL runs through Zink (PLA-162, done).
- Don't give up quickly; don't reinvent the wheel. Use ProtonDB, Proton's own per-app table
  (`default_compat_config()` in the `proton` script on the workstation) and desktop Proton
  (`docs/HOST-DEBUGGING.md`). Mirror what fits as a compat default; name only "Proton".
- Videos invisible on Vulkan get fixed on first sight ([PLA-121](https://linear.app/playportdev/issue/PLA-121)).
- Leave out titles already tested and playing; tested-but-Broken titles stay in.
- Wine Mono ships, as Proton does (decision 0077).
- Prerequisites: Play runs a title's own prerequisite installer once per prefix (decision
  0079). For Steam games Playport may also download the Steamworks Common Redistributables
  (app 228980) depot as Steam does, when the game's `installscript.vdf` lists it (decision
  0081, owner 2026-10-11). Nothing redistributable is bundled in the IPA.
- The prefix base must match a fresh wineboot/Proton template (decision 0080); gaps are found
  by a host diff, not game by game. One shared prefix (owner 2026-10-12; per-title prefixes,
  PLA-169, canceled).
- Invisible windows first: guest GDI windows (launchers, setup and error dialogs) are shown
  (decisions 0075, 0076).
- Every unfixed finding gets its own Linear issue linked to the titles it blocks.

## The worker brief

Each per-game worker gets the brief left in the build area
(`.work/agent-notes/speedrun/brief.md`): the rules above, phone mechanics (install, space,
lock, the iOS alert check), lean paperwork and a fixed report format. Tell it the branch the
library run uses. Points learned the hard way:

- **Every `pp ui` gets `--wait 180`** (or less). A game on an error box never reaches
  `first-frame`; use `--until "mark:first window+20" --shot` and read `[msgbox]`/`[win-text]`
  in the run's `pull/s1-host.log`. When a Play runs prerequisites, their console window is the
  first window: use `mark:game started+N`. An unbounded run held the phone 28 min (2026-10-11).
- `--settings` `"arguments"` is a string. `wait:`/`pad:` actions after `play:` don't run
  ([PLA-156](https://linear.app/playportdev/issue/PLA-156)): use `pp ui --pad … --leave-running`
  and `pp pad send` in one `pp phone lock` session. A guest window is driven with `tap:FX:FY`
  or pad keys (A=Enter, B=Escape, D-pad=arrows).
- **Never wait for free space (owner, 2026-10-11).** If a title doesn't fit, uninstall
  installed titles through the UI (`pp ui --action uninstall:ID --wait 180`) until it does:
  first titles already finished in this run, then any title not needed for current testing,
  largest first if that frees enough in one go. Record what was removed in the report and in
  Phone state below. Never the app; keep Hollow Knight and Portal 2 (the smoke checks) unless
  nothing else frees enough; House of Golf 2 may go if needed (reinstall later). installd fails
  with `PackageExtractionFailed` below ~2.25 GB free, so leave that margin. Leave the owner's
  files in Documents (e.g. `Playport-PLA-147.ipa`).
- A Unity Mono title dying at start with a garbage exception code: try runtime
  `env.MADEIRA_KEEP_SMC=1` ([PLA-115](https://linear.app/playportdev/issue/PLA-115), fixed 2026-10-13; Duck Paradox no longer needs it).
- An iOS system alert over the app stalls every play and can't be dismissed remotely
  ([PLA-119](https://linear.app/playportdev/issue/PLA-119)); check
  `pymobiledevice3 developer accessibility list-items` and stop for the owner.
- Worker reports must end with "Games observed"; the supervisor writes Linear from them.

## Progress

## Done so far

- Phase 1, the Compatibility issues we could test, and the issue sweep (every issue filed during
  the runs, PLA-113 onwards, plus the High backlog) are finished (2026-10-08 to 2026-10-13).
  Their per-chunk log, with commits, decisions 0075–0104 and evidence, is in this file's history
  up to commit 1e11bac (`git show 1e11bac:docs/plans/2026-10-08-compat-speedrun.md`); the
  per-chunk reports are in the build area (below).
- Left open from the sweep, unscheduled: the A12 profile ([PLA-320](https://linear.app/playportdev/issue/PLA-320), [PLA-315](https://linear.app/playportdev/issue/PLA-315), [PLA-132](https://linear.app/playportdev/issue/PLA-132)),
  [PLA-303](https://linear.app/playportdev/issue/PLA-303), [PLA-308](https://linear.app/playportdev/issue/PLA-308), [PLA-309](https://linear.app/playportdev/issue/PLA-309), [PLA-311](https://linear.app/playportdev/issue/PLA-311), [PLA-313](https://linear.app/playportdev/issue/PLA-313), [PLA-314](https://linear.app/playportdev/issue/PLA-314), [PLA-306](https://linear.app/playportdev/issue/PLA-306), [PLA-321](https://linear.app/playportdev/issue/PLA-321), [PLA-322](https://linear.app/playportdev/issue/PLA-322),
  [PLA-323](https://linear.app/playportdev/issue/PLA-323), [PLA-319](https://linear.app/playportdev/issue/PLA-319); [PLA-119](https://linear.app/playportdev/issue/PLA-119) and [PLA-219](https://linear.app/playportdev/issue/PLA-219) wait for a live system alert; [PLA-8](https://linear.app/playportdev/issue/PLA-8) waits for a
  reporter's `jit:` line. Timed replays not run: Football Manager 2022 ([PLA-114](https://linear.app/playportdev/issue/PLA-114)) and JWE ([PLA-41](https://linear.app/playportdev/issue/PLA-41)).

### Titles (latest result per title; Linear Compatibility issue)

| Title | Issue | Result (latest) | Fixes on the way |
| --- | --- | --- | --- |
| #DRIVE Rally (2494780) | PLA-178 | Works (Vulkan 16:9, welcome screen at first frame + 30) | none needed (d6217b0) |
| House of Golf 2 (Epic) | PLA-67 | Playable with issues: title screen after ~4 min; menus/play not checked | PLA-125, -145, -144, -113, wine-unix 0026/0027 |
| FM2022 Resource Archiver (Epic) | PLA-66 | Works (needs FM2022 installed) | 1c20a0c |
| Football Manager 2022 (Epic) | PLA-114 | Broken: small window at +6 s, no frame in 180 s; anti-tamper crash gone after wine-pe 0038; 24-hour re-validation lockout, replay after 2026-10-13 16:00 | 1c20a0c, af4be23, 9d40a49 |
| Monster Train (GOG) | PLA-54 | Works | 295e18a |
| Soccer Online: Ball 3D | PLA-53 | Works | f7bd7a1 |
| Duck Paradox (GOG) | PLA-52 | Works, no compat default since d456d5e (PLA-115 fixed) | d456d5e |
| Jurassic World Evolution (Epic) | PLA-41 | Broken: first frame at +38 s, then its WatchDog at 74 s (split locks, fixed in 11m, not yet replayed: re-validation `88500006` until ~2026-10-13 22:30) | ca7289c |
| Among Us | PLA-39 | Works | ed9191f |
| DiRT 3 Complete Edition (Local, 321040) | PLA-24 | Playable with issues: Vulkan (D3D11) 16:9, title screen at first frame + 30, pad to the New Profile menu; exe set by hand ([PLA-322](https://linear.app/playportdev/issue/PLA-322)) | none needed (90f2e68) |
| Mortal Kombat Komplete Edition (Local) | PLA-36 | Playable with issues: defaults, menus to a pad fight at ~53 FPS | 380c5c0, 0c51652, c3cda59, 60d6de9 (fex 0025) |
| Tomb Raider (203160) | PLA-150 | Playable with issues: launcher shown, Play → main menu | PLA-152/151 |
| Castle Crashers Demo (207100) | PLA-159 | Works (Vulkan, i386 D3D9); full game PLA-34 likely blocked by SteamStub x86 (PLA-99) | none |
| Cat Mail Co. Demo (4622530) | PLA-160 | Works on Vulkan (default): tutorial drawn right at 60 FPS | fee230f (NSI UDP table), 246365a (mesa 0030, PLA-161) |
| Neverwinter Nights: EE (GOG 1097893768) | PLA-185 | Playable with issues: gameplay through OpenGL/Zink at 50 fps; fullscreen needs a `settings.tml` edit | 22e2abc |
| Baldur's Gate: EE (GOG 1207666353) | PLA-183 | Playable with issues: main menu on OpenGL (Zink) after GOG's OpenAL redist; gameplay and sound unchecked | ad2fce2 (PLA-182, 0090) |
| Neverwinter Nights Diamond (GOG 1207658890) | PLA-187 | Past its setup (11b); Bink intro and menu show (11e); gameplay unchecked | 11b, 11e |
| BioShock (GOG 2022341186) | PLA-248 | Works: menu scene on Vulkan 16:9 | 7bec31e, a3b59b4 |
| Tomb Raider: Anniversary (GOG 1202885143) | PLA-249 | Works: setup dialog, Ok, intro to first frame + 30 | a3b59b4, 9540b2e |
| Spelunky (GOG 1207659257) | PLA-247 | Works (launch) | 474934a |
| Quake II (GOG 1441704824) | PLA-188 | Not installable from GOG in the app | — |
| Counter-Strike (10) | PLA-22 | Playable with issues: OpenGL (`-gl`) de_dust2 at 59.7 FPS, no input tested; `-soft` broken (PLA-164) | e5a928b (PLA-9), PLA-162, 9a2906c |
| Half-Life (70) | PLA-25 | Playable with issues: OpenGL c1a0 at 60 FPS, GPU 12–15 ms (PLA-191); can't share its folder with CS (PLA-192) | PLA-162, 9a2906c |

Compat defaults in `app/PlayportKit/Titles/compat-defaults.json`: Super Meat Boy Epic `aspect: 16:9` (Duck Paradox's `env.MADEIRA_KEEP_SMC=1` removed in d456d5e, PLA-115).

## Running now

Nothing.

## Queue

1. **The library A–Z** (owner, 2026-10-13: the only remaining item). #1 #DRIVE Rally is done;
   next is **9 Years of Shadows** (GOG 2099051765). Use the triage in the build area
   (`.work/agent-notes/speedrun/library-triage.md`: ProtonDB, anti-cheat, API, size, verdict per
   title) and the PLA-163 import census (`.work/agent-notes/pla163/census.md`). Skip SKIP
   verdicts, empty depots, dead services, and a second store copy once one is tested. One
   per-game worker per title (brief above); the supervisor creates the Linear issue from the
   report. At Dishonored, run its GOG `.msi` on the phone ([PLA-250](https://linear.app/playportdev/issue/PLA-250)) if space allows by the
   space rule.

Not yet scheduled: an XNA title to first frame + 30 s (Mono's SMC cost under i386 FEX).

Owner decisions open: [PLA-155](https://linear.app/playportdev/issue/PLA-155) (Wine Mono size), [PLA-176](https://linear.app/playportdev/issue/PLA-176) (`__PAGEZERO` experiment).

## Phone state (2026-10-13 after DiRT 3)

Unattended mode on. Installed: Hollow Knight, Portal 2, Counter-Strike, Jurassic World Evolution,
Death's Door (Epic), Valheim, Neverwinter Nights Diamond (GOG), DiRT 3 Complete Edition (Local),
and small probe imports (`dir-wgl`, `dir-cpuid*`, `dir-childabort*`, …). Removed through the UI on
2026-10-13 for space: BioShock, Tomb Raider: Anniversary, Spelunky, Football Manager 2022 (the
FM2022 Resource Archiver entry needs FM2022 installed). About 5.7 GB free. Latest IPA 7e95a2c0.

## Where things are

- Build-area queue (kept in step with this plan, which wins): `.work/agent-notes/speedrun/queue.md`.
- Per-game and per-chunk reports: `.work/agent-notes/speedrun/reports/` (copies of each child's
  report; `p2-NN-<slug>.md` for Phase 2 titles).
- Brief: `.work/agent-notes/speedrun/brief.md`. Library triage:
  `.work/agent-notes/speedrun/library-triage.md`. Census and prefix diff: `.work/agent-notes/pla163/`.

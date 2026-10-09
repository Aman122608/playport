# Plan: compatibility speedrun

**Date:** 2026-10-08 (started), paused 2026-10-09. **Kind:** execution plan, in progress.
**Linear:** [PLA-112](https://linear.app/playportdev/issue/PLA-112) (umbrella; each game has its
own Compatibility issue, each cross-title fault its own issue).
**Branch:** `dev/compat-speedrun` (from `main` 5fc71ff), unmerged. Last installed dev IPA
`ddb449f65cdfdbdf2d8241191a5e2c38a264172bf950bb07d6f82c659b1a8fbc` (0fb16a3).

## Goal

As many titles playable as possible: first the Linear Compatibility issues we can test, then
the owner's library alphabetically. Proton parity is the target: what a title gets under
desktop Proton (prefix contents, Wine Mono, redistributables it ships) it should get here.

## Settled (owner, 2026-10-08/09)

- One quick worker per game, run by the supervisor in the plan-runner manner; Opus 5.5 medium.
- Vulkan first (DXVK/vkd3d-proton). DXMT only when Vulkan can't be made to work; record why.
- Every play at 16:9 (decision 0073). A setting a title needs by default goes into
  `app/PlayportKit/Titles/compat-defaults.json` (decision 0074).
- Done = first frame + 30 s with something visible on a screenshot; write the game's Linear
  issue and `site/compatibility/games.json`; next game.
- No Hollow Knight gate or regression runs per game. Exception: a change to window creation,
  the prefix or the fault path gets a quick Hollow Knight/Portal 2 first-frame+10.
- Don't assume unplayable (launchers, online, store sign-in). Skip only anti-cheat that blocks
  Linux. Stop at unimplemented graphics APIs (OpenGL, an i386 D3D10/11 backend).
- Don't give up quickly; don't reinvent the wheel. Use ProtonDB and desktop Proton on the
  workstation (`docs/HOST-DEBUGGING.md`).
- Videos invisible on Vulkan get fixed on first sight ([PLA-121](https://linear.app/playportdev/issue/PLA-121)).
- Leave out titles already tested and playing; tested-but-Broken titles stay in.
- Wine Mono is shipped, as Proton does ([PLA-125](https://linear.app/playportdev/issue/PLA-125)).
- Playport never ships or downloads Microsoft's redistributables; it may run the copy a title
  ships with, once, on the device, as Steam/Heroic do ([PLA-113](https://linear.app/playportdev/issue/PLA-113)).
- Every unfixed finding gets its own Linear issue linked to the titles it blocks.

## The worker brief

Each per-game worker gets the brief left in the build area
(`.work/agent-notes/speedrun/brief.md`): the rules above, phone mechanics (install, space,
lock, the iOS alert check), lean paperwork and a fixed report format. Points learned the hard way:

- `--settings` `"arguments"` is a string. `pad:` actions don't run during play: use
  `pp ui --pad … --leave-running` and `pp pad send` in one `pp phone lock` session.
- Pass `--wait` for titles that may hang; free space by uninstalling finished titles (never
  the app, Hollow Knight or Portal 2).
- A Unity Mono title dying at start with a garbage exception code: try runtime
  `env.MADEIRA_KEEP_SMC=1` ([PLA-115](https://linear.app/playportdev/issue/PLA-115)).
- An iOS system alert over the app stalls every play and can't be dismissed remotely
  ([PLA-119](https://linear.app/playportdev/issue/PLA-119)); check
  `pymobiledevice3 developer accessibility list-items` and stop for the owner.

## Progress (2026-10-09)

| Title | Issue | Result | Fix on the way |
| --- | --- | --- | --- |
| House of Golf 2 (Epic) | PLA-67 | Broken: needs MFC from its own prereq installer | b216243, 393115d, 7d05797 |
| FM2022 Resource Archiver (Epic) | PLA-66 | Works (needs FM2022 installed) | 1c20a0c |
| Football Manager 2022 (Epic) | PLA-114 | Broken: per-page protect too slow (PLA-118) | 1c20a0c |
| Monster Train (GOG) | PLA-54 | Works | 295e18a |
| Soccer Online: Ball 3D | PLA-53 | Works | f7bd7a1 |
| Duck Paradox (GOG) | PLA-52 | Works (compat default) | 7c2a71f |
| Jurassic World Evolution (Epic) | PLA-41 | Broken: DRM wild store (PLA-122) | ca7289c |
| Among Us | PLA-39 | Works | ed9191f |
| Mortal Kombat Komplete Edition (Local) | PLA-36 | Broken: dxdiag (PLA-117), allocator deadlock (PLA-120) | 380c5c0 |

Cross-title fixes, all on the branch: VC runtime and wintrust SIP registry; `ucrtbase` from
system32; the audio-timer system thread (videos, event WASAPI); monitor `\\.\DISPLAY1`; commit
probe and alignment faults; Wine's battery state; no forced PROT_EXEC for non-NX_COMPAT exes;
`drivers\etc` (PLA-116, done); builtin window classes per process; prefix winsxs; the WoW64
sent-message pointer. Re-checked on the latest IPAs: Hollow Knight, Portal 2, Super Meat Boy,
Among Us.

## Next

1. **PLA-125, Wine Mono** (11.3.0 for the Wine 11.19 pin; facts in the issue).
2. **PLA-145, Authenticode signer chain** for Microsoft-signed payloads, then
   [PLA-144](https://linear.app/playportdev/issue/PLA-144) (Burn doesn't exit), then wire
   PLA-113's run-the-title's-prereq-once step and replay House of Golf 2.
3. **Phase 1, remaining titles:** Castle Crashers Demo (207100; the i386 D3D9 class for PLA-34/35/38),
   Cat Mail Co. Demo (4622530, PLA-27), Counter-Strike (10; GoldSrc software/D3D, PLA-22/25),
   DiRT 3 Complete Edition (Local import, D3D9 path, PLA-24).
4. **Phase 2:** the library A–Z from the triage left in the build area
   (`.work/agent-notes/speedrun/library-triage.md`: ProtonDB, anti-cheat, API, size, verdict
   per title). Skip SKIP verdicts, empty depots, dead services and a second store copy once
   one is tested.

The live queue with per-title notes is `.work/agent-notes/speedrun/queue.md`; per-game
reports are in `.work/agent-notes/speedrun/reports/`.

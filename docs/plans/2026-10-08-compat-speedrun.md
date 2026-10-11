# Plan: compatibility speedrun

**Date:** 2026-10-08 (started); paused 2026-10-09; resumed 2026-10-10/11. **Kind:** execution
plan, in progress. **Linear:** [PLA-112](https://linear.app/playportdev/issue/PLA-112) (umbrella;
each game has its own Compatibility issue, each cross-title fault its own issue).

**Branch:** `dev/pla-152-gdi-windows` (from `main` 6ad89e8), unmerged and unpushed; it carries
all of the speedrun's work since 2026-10-10. (`dev/compat-speedrun`, the first leg, is merged
into `main`.) Merge to `main` with `--no-ff` only when the owner asks.

## How to pick this up in a new session

1. Read this file, then `git log --oneline main..dev/pla-152-gdi-windows` and Linear PLA-112.
2. The supervisor runs it with the plan-runner skill (`.pi/skills/plan-runner`): ask the owner
   for the hard/easy models, one writer at a time on this branch, the per-game brief below.
3. Check `git status` (clean) and whether a writer is mid-chunk (Linear issues In Progress,
   the last report in `.work/agent-notes/speedrun/reports/`). Start at **Next** item 1.
4. Phone: `./pp phone status` (free space ≥ 5 GB for installs; see Phone state below).

## Goal

As many titles playable as possible: first the Linear Compatibility issues we can test, then
the owner's library alphabetically. **Proton parity is the target** (owner, 2026-10-11): if a
title runs under Proton without protontricks, it should run here; a difference is our bug.

## Settled (owner, 2026-10-08 to 2026-10-11)

- One quick worker per game, run by the supervisor in the plan-runner manner. Models chosen
  per session (2026-10-10/11: hard = Opus 5.5 medium, easy = Sonnet 5.5 medium).
- Vulkan first (DXVK/vkd3d-proton). DXMT only when Vulkan can't be made to work; record why
  and file the Vulkan bug.
- Every play at 16:9 (decision 0073). A setting a title needs by default goes into
  `app/PlayportKit/Titles/compat-defaults.json` (decision 0074).
- Done = first frame + 30 s with something visible on a screenshot; write the game's Linear
  issue and `site/compatibility/games.json`; next game.
- No Hollow Knight gate per game. Exception: a change to window creation, the prefix, the
  launch path or the fault path gets a quick Hollow Knight (`app-367520`) and Portal 2
  (`app-620`) first-frame+10.
- Don't assume unplayable (launchers, online, store sign-in). Skip only anti-cheat that blocks
  Linux. OpenGL is next to be implemented (Zink, below); until then an OpenGL-only title is
  reported "Broken: OpenGL (PLA-162)" after trying its non-GL modes.
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
  by a host diff, not game by game. One shared prefix for now; per-title prefixes are an open
  question ([PLA-169](https://linear.app/playportdev/issue/PLA-169)), no change until the owner decides.
- Invisible windows first: guest GDI windows (launchers, setup and error dialogs) are shown
  (decisions 0075, 0076).
- Every unfixed finding gets its own Linear issue linked to the titles it blocks.

## The worker brief

Each per-game worker gets the brief left in the build area
(`.work/agent-notes/speedrun/brief.md`): the rules above, phone mechanics (install, space,
lock, the iOS alert check), lean paperwork and a fixed report format. Tell it the branch above
(the brief names it). Points learned the hard way:

- **Every `pp ui` gets `--wait 180`** (or less). A game on an error box never reaches
  `first-frame`; use `--until "mark:first window+20" --shot` and read `[msgbox]`/`[win-text]`
  in the run's `pull/s1-host.log`. When a Play runs prerequisites, their console window is the
  first window: use `mark:game started+N`. An unbounded run held the phone 28 min (2026-10-11).
- `--settings` `"arguments"` is a string. `wait:`/`pad:` actions after `play:` don't run
  ([PLA-156](https://linear.app/playportdev/issue/PLA-156)): use `pp ui --pad … --leave-running`
  and `pp pad send` in one `pp phone lock` session. A guest window is driven with `tap:FX:FY`
  or pad keys (A=Enter, B=Escape, D-pad=arrows).
- Free space by uninstalling finished titles through the UI (never the app, Hollow Knight,
  Portal 2 or House of Golf 2). installd fails with `PackageExtractionFailed` below ~2.25 GB.
  Leave the owner's files in Documents (e.g. `Playport-PLA-147.ipa`).
- A Unity Mono title dying at start with a garbage exception code: try runtime
  `env.MADEIRA_KEEP_SMC=1` ([PLA-115](https://linear.app/playportdev/issue/PLA-115)).
- An iOS system alert over the app stalls every play and can't be dismissed remotely
  ([PLA-119](https://linear.app/playportdev/issue/PLA-119)); check
  `pymobiledevice3 developer accessibility list-items` and stop for the owner.
- Worker reports must end with "Games observed"; the supervisor writes Linear from them.

## Progress

### Titles

| Title | Issue | Result (latest) | Fixes on the way |
| --- | --- | --- | --- |
| House of Golf 2 (Epic) | PLA-67 | Playable with issues: title screen after ~4 min; menus/play not checked | PLA-125, -145, -144, -113, wine-unix 0026/0027 |
| FM2022 Resource Archiver (Epic) | PLA-66 | Works (needs FM2022 installed) | 1c20a0c |
| Football Manager 2022 (Epic) | PLA-114 | Broken: per-page protect too slow (PLA-118) | 1c20a0c |
| Monster Train (GOG) | PLA-54 | Works | 295e18a |
| Soccer Online: Ball 3D | PLA-53 | Works | f7bd7a1 |
| Duck Paradox (GOG) | PLA-52 | Works (compat default) | 7c2a71f |
| Jurassic World Evolution (Epic) | PLA-41 | Broken: DRM wild store (PLA-122) | ca7289c |
| Among Us | PLA-39 | Works | ed9191f |
| Mortal Kombat Komplete Edition (Local) | PLA-36 | Broken: dxdiag (PLA-117), allocator deadlock (PLA-120) | 380c5c0 |
| Tomb Raider (203160) | PLA-150 | Playable with issues: launcher shown, Play → main menu | PLA-152/151 |
| Castle Crashers Demo (207100) | PLA-159 | Works (Vulkan, i386 D3D9); full game PLA-34 likely blocked by SteamStub x86 (PLA-99) | none |
| Cat Mail Co. Demo (4622530) | PLA-160 | Works on DXMT (compat default); Vulkan 3D washed out (PLA-161) | fee230f (NSI UDP table) |
| Counter-Strike (10) | PLA-22 | Broken: installs and starts; `-soft` first frame then host fault (PLA-164); OpenGL default (PLA-162) | e5a928b (PLA-9) |

### Cross-title work on `dev/pla-152-gdi-windows` (2026-10-10/11)

| Commit(s) | Issue | What | Record |
| --- | --- | --- | --- |
| 4e64178, d897a53 | PLA-152, PLA-153 | Guest GDI windows shown before the first frame and as dialogs after it; touch and pad drive them | decisions 0075, 0076; evidence 2026-10-11-gdi-windows |
| 1a10332, d85b050 | PLA-125 | Wine Mono 11.3.0 shipped, its registry seeded; x86-64 .NET runs | decision 0077; evidence 2026-10-11-wine-mono |
| ee64479 | PLA-145, PLA-144 | wintrust/msisip registration (WinVerifyTrust works); installer writes over runtime links; WoW64 window lock at exit | decision 0078; evidence 2026-10-11-uepre-authenticode |
| 71e099b, f12abad | PLA-113 | Title prerequisites once from Play (Epic PrereqPath, UE `UEPrereqSetup_x64`); per-thread `zero_bits` | decision 0079; evidence 2026-10-11-prerequisites-from-play |
| fee230f | PLA-27 | NSI UDP endpoint table served in-process | evidence 2026-10-11-nsi-udp-endpoints |
| e5a928b | PLA-9 | Steam `depotfromapp` depots installed; Steam launch-entry arguments; arguments quoted only when needed | — |
| c319ca7 | PLA-162 | Zink plan written | `docs/plans/2026-10-11-opengl-zink.md` |
| da1e1a3, 9cd12c7 | PLA-163 | Prefix seed = wineboot template (wine.inf AddReg, urlmon zones, media registrations, i386-only classes); required-registration test; Wow64 emulator key left out | decisions 0080, 0081; evidence 2026-10-11-prefix-parity |

Re-checked on the branch's IPAs after each runtime change: Hollow Knight and Portal 2 reach
their first frame (latest on the 9cd12c7 build, with House of Golf 2).

## Next

1. **[PLA-158](https://linear.app/playportdev/issue/PLA-158), Steam `installscript.vdf`
   prerequisites + 228980 download (decision 0081): done 2026-10-11** (bb5a281, with 9cd12c7 for the PLA-163 seed fix
   it needed first; [evidence](../evidence/2026-10-11-steam-install-scripts.md)).
   The mechanism works on the phone: Castle Crashers Demo's Play downloaded DirectX June 2010
   (228990, 103 MB in 3.2 s) into the shared `Games/Steamworks Shared`, ran `DXSETUP.exe /silent`
   once, recorded it and skipped it next time; Counter-Strike added VC++ 2019 (228988). Both
   installers fail in the runtime, not in the mechanism: DXSETUP at its Managed DirectX check
   (`LoadLibraryShim` 0x80070006, new issue) and VC++ 2015+ `.cmd` files for want of
   `system32\start.exe` ([PLA-165](https://linear.app/playportdev/issue/PLA-165)), so no HasRunKey
   was written on the phone yet (host-tested). The Proton-prefix comparison is in the evidence.
2. **Phase 1 rest: DiRT 3 Complete Edition** (Local import, 15 GB, i386 D3D9 path,
   [PLA-24](https://linear.app/playportdev/issue/PLA-24)): blocked by `openal32`, which its own
   `_CommonRedist/OpenAL` installscript installs (item 1). Needs ~17 GB free on the phone.
3. **Cross-title runtime fixes found this leg**, in order of reach:
   [PLA-165](https://linear.app/playportdev/issue/PLA-165) (64-bit programs missing from system32:
   **done 2026-10-11**, every Wine program but winedbg/wineboot/explorer built for arm64ec, decision 0082;
   [evidence](../evidence/2026-10-11-system32-programs.md), HasRunKey write proven with Lossless Scaling's VC++ 2022),
   [PLA-171](https://linear.app/playportdev/issue/PLA-171) (a failed prerequisite reruns once per new app build;
   Steamworks Shared in Settings › Storage: **done 2026-10-11**, decision 0083),
   [PLA-172](https://linear.app/playportdev/issue/PLA-172) (prerequisites ate the game session's JIT pool,
   225/512 MiB for Counter-Strike's VC++ pair: **done 2026-10-11**, a Play whose prerequisites ran restarts
   Playport and starts the game in the new process, head 20 MiB at the game's start, decision 0084;
   [evidence](../evidence/2026-10-11-prerequisites-before-a-restart.md)),
   [PLA-170](https://linear.app/playportdev/issue/PLA-170) with [PLA-154](https://linear.app/playportdev/issue/PLA-154)
   (i386 .NET/mscoree: DXSETUP's Managed DirectX check, XNA titles: **done 2026-10-11**, an i386 .NET
   program runs and DXSETUP passes the managed DirectX check, decision 0085; DXSETUP then stops at the
   x64 `infinst.exe`'s fixed base `0x100000000`; [evidence](../evidence/2026-10-11-i386-dotnet.md)),
   [PLA-174](https://linear.app/playportdev/issue/PLA-174) (x64 relocation-stripped exe at
   `0x100000000`, which blocks every DirectX Jun2010 DXSETUP: **done 2026-10-11** as an iOS limit.
   PIE page zero ends at 4 GiB plus the slide, so the range can never be mapped. No layout
   change was made, and Proton seeds no DirectX HasRunKey; a smaller `__PAGEZERO` is left to the
   owner. [Evidence](../evidence/2026-10-11-fixed-base-4g.md)),
   [PLA-175](https://linear.app/playportdev/issue/PLA-175) (no 32-bit registry view for i386 callers:
   **done 2026-10-11** as an audit, no change: i386 callers read the one 64-bit view, the Program Files
   paths and `InstallRoot` already reach them right, a `REQUIRED_I386` host test guards it; a program
   creating `Software\Wow6432Node` would hide `HKLM\Software` and HKCR from them, a follow-up;
   [evidence](../evidence/2026-10-11-wow64-registry-view.md)),
   **next:**
   an XNA title to first frame + 30 s (Mono's self-modifying-code cost under i386 FEX),
   [PLA-164](https://linear.app/playportdev/issue/PLA-164) (WoW64 host fault at a stack end; Counter-Strike),
   [PLA-173](https://linear.app/playportdev/issue/PLA-173) (winedbg for arm64ec: x64 crash backtraces),
   [PLA-161](https://linear.app/playportdev/issue/PLA-161) (Vulkan Unity 6 URP washed out),
   [PLA-99](https://linear.app/playportdev/issue/PLA-99) (SteamStub 3.1 x86).
4. **OpenGL via Zink, [PLA-162](https://linear.app/playportdev/issue/PLA-162):** its own plan
   ([2026-10-11-opengl-zink](2026-10-11-opengl-zink.md)), run after items 1–2.
5. **Phase 2:** the library A–Z from the triage left in the build area
   (`.work/agent-notes/speedrun/library-triage.md`: ProtonDB, anti-cheat, API, size, verdict per
   title) and the PLA-163 import census (`.work/agent-notes/pla163/census.md`: missing DLLs and
   prerequisite source per installed title). Skip SKIP verdicts, empty depots, dead services and
   a second store copy once one is tested.

Smaller open items: [PLA-155](https://linear.app/playportdev/issue/PLA-155) (Wine Mono size,
owner decision), [PLA-157](https://linear.app/playportdev/issue/PLA-157),
[PLA-166](https://linear.app/playportdev/issue/PLA-166), [PLA-167](https://linear.app/playportdev/issue/PLA-167),
[PLA-168](https://linear.app/playportdev/issue/PLA-168), [PLA-169](https://linear.app/playportdev/issue/PLA-169)
(owner question). The owner still signs off the release app's launcher/dialog behaviour
(decision 0068) before a merge.

## Phone state (2026-10-11)

Unattended mode on. Installed: Hollow Knight, Portal 2, House of Golf 2 (21.9 GB); everything
else tested was uninstalled through the UI (incl. Counter-Strike, Cat Mail Co. Demo, Tomb Raider,
Castle Crashers Demo, Lossless Scaling) and `Steamworks Shared` was removed in Storage. About
14.7 GB free (11 Oct, 11:15); check `pp phone status` before any install. The prefix records
failed prerequisites for Castle Crashers' DXSETUP and Counter-Strike's VC++ 2019 (1638,
downgrade refused because HoG2 installed 14.38) and the VC++ 2022 HasRunKeys.

## Where things are

- Live queue with per-title notes: `.work/agent-notes/speedrun/queue.md`.
- Per-game and per-chunk reports: `.work/agent-notes/speedrun/reports/`.
- Brief: `.work/agent-notes/speedrun/brief.md`. Census and prefix diff: `.work/agent-notes/pla163/`.

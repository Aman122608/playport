# Plan: compatibility speedrun

**Date:** 2026-10-08 (started); paused 2026-10-09; resumed 2026-10-10/11. **Kind:** execution
plan, in progress. **Linear:** [PLA-112](https://linear.app/playportdev/issue/PLA-112) (umbrella;
each game has its own Compatibility issue, each cross-title fault its own issue).

**Branch:** `dev/pla-152-gdi-windows` (from `main` 6ad89e8), unmerged and unpushed; it carries
all of the speedrun's work since 2026-10-10. (`dev/compat-speedrun`, the first leg, is merged
into `main`.) Merge to `main` with `--no-ff` only when the owner asks. Last updated
2026-10-11 13:40 at 164d44b (+ this plan commit).

## How to pick this up in a new session

1. Read this file, then `git log --oneline main..dev/pla-152-gdi-windows` and Linear PLA-112.
2. The supervisor runs it with the plan-runner skill (`.pi/skills/plan-runner`): ask the owner
   for the hard/easy models, one writer at a time on this branch, the per-game brief below.
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
- **Never wait for free space (owner, 2026-10-11).** If a title doesn't fit, uninstall
  installed titles through the UI (`pp ui --action uninstall:ID --wait 180`) until it does:
  first titles already finished in this run, then any title not needed for current testing,
  largest first if that frees enough in one go. Record what was removed in the report and in
  Phone state below. Never the app; keep Hollow Knight and Portal 2 (the smoke checks) unless
  nothing else frees enough; House of Golf 2 may go if needed (reinstall later). installd fails
  with `PackageExtractionFailed` below ~2.25 GB free, so leave that margin. Leave the owner's
  files in Documents (e.g. `Playport-PLA-147.ipa`).
- A Unity Mono title dying at start with a garbage exception code: try runtime
  `env.MADEIRA_KEEP_SMC=1` ([PLA-115](https://linear.app/playportdev/issue/PLA-115)).
- An iOS system alert over the app stalls every play and can't be dismissed remotely
  ([PLA-119](https://linear.app/playportdev/issue/PLA-119)); check
  `pymobiledevice3 developer accessibility list-items` and stop for the owner.
- Worker reports must end with "Games observed"; the supervisor writes Linear from them.

## Progress

### Titles (latest result per title; Linear Compatibility issue)

| Title | Issue | Result (latest) | Fixes on the way |
| --- | --- | --- | --- |
| #DRIVE Rally (2494780) | PLA-178 | Works (Vulkan 16:9, welcome screen at first frame + 30) | none needed (d6217b0) |
| House of Golf 2 (Epic) | PLA-67 | Playable with issues: title screen after ~4 min; menus/play not checked | PLA-125, -145, -144, -113, wine-unix 0026/0027 |
| FM2022 Resource Archiver (Epic) | PLA-66 | Works (needs FM2022 installed) | 1c20a0c |
| Football Manager 2022 (Epic) | PLA-114 | Broken: per-page protect too slow (PLA-118) | 1c20a0c |
| Monster Train (GOG) | PLA-54 | Works | 295e18a |
| Soccer Online: Ball 3D | PLA-53 | Works | f7bd7a1 |
| Duck Paradox (GOG) | PLA-52 | Works (compat default `MADEIRA_KEEP_SMC=1`, PLA-115) | 7c2a71f |
| Jurassic World Evolution (Epic) | PLA-41 | Broken: DRM wild store (PLA-122) | ca7289c |
| Among Us | PLA-39 | Works | ed9191f |
| Mortal Kombat Komplete Edition (Local) | PLA-36 | Broken: dxdiag (PLA-117), allocator deadlock (PLA-120); not retried since | 380c5c0 |
| Tomb Raider (203160) | PLA-150 | Playable with issues: launcher shown, Play → main menu | PLA-152/151 |
| Castle Crashers Demo (207100) | PLA-159 | Works (Vulkan, i386 D3D9); full game PLA-34 likely blocked by SteamStub x86 (PLA-99) | none |
| Cat Mail Co. Demo (4622530) | PLA-160 | Works on DXMT (compat default); Vulkan 3D washed out (PLA-161) | fee230f (NSI UDP table) |
| Counter-Strike (10) | PLA-22 | Broken: installs and starts; `-soft` first frame then host fault (PLA-164); OpenGL default (PLA-162) | e5a928b (PLA-9) |

Compat defaults in `app/PlayportKit/Titles/compat-defaults.json`: Cat Mail Co. Demo `graphics: dxmt`
(PLA-161), Super Meat Boy Epic `aspect: 16:9`, Duck Paradox `env.MADEIRA_KEEP_SMC=1` (PLA-115).

### Cross-title work on `dev/pla-152-gdi-windows` (2026-10-10/11)

| Commit(s) | Issue | What | Record |
| --- | --- | --- | --- |
| 4e64178, d897a53 | PLA-152, PLA-153 | Guest GDI windows (launchers, dialogs) shown before the first frame and as dialogs after it; touch and pad drive them | decisions 0075, 0076; evidence 2026-10-11-gdi-windows |
| 1a10332, d85b050 | PLA-125 | Wine Mono 11.3.0 shipped (+107 MB IPA), its registry seeded; x86-64 .NET runs | decision 0077; evidence 2026-10-11-wine-mono |
| ee64479 | PLA-145, PLA-144 | wintrust/msisip registration; installer writes over runtime links; WoW64 window lock at exit | decision 0078; evidence 2026-10-11-uepre-authenticode |
| 71e099b, f12abad | PLA-113 | A title's own prerequisites once from Play (Epic PrereqPath, UE `UEPrereqSetup_x64`); per-thread `zero_bits` | decision 0079; evidence 2026-10-11-prerequisites-from-play |
| fee230f | PLA-27 | NSI UDP endpoint table served in-process | evidence 2026-10-11-nsi-udp-endpoints |
| e5a928b | PLA-9 | Steam `depotfromapp` depots; Steam launch-entry arguments; quoting only when needed | — |
| c319ca7 | PLA-162 | Zink plan written | `docs/plans/2026-10-11-opengl-zink.md` |
| da1e1a3, 9cd12c7 | PLA-163 | Prefix seed = wineboot template; required-registration test; Wow64 emulator key left out | decisions 0080, 0081; evidence 2026-10-11-prefix-parity |
| bb5a281 | PLA-158 | Steam `installscript.vdf` prerequisites; Steamworks Common Redistributables (228980) downloaded once into `Games/Steamworks Shared`; HasRunKeys written | decision 0081; evidence 2026-10-11-steam-install-scripts |
| e54bf70 | PLA-165 | Every Wine program built for arm64ec (start, reg, conhost…; not winedbg/wineboot/explorer); +3.4 MB IPA | decision 0082; evidence 2026-10-11-system32-programs |
| 5988943 | PLA-171 | A failed prerequisite reruns once per new app build; Steamworks Shared in Settings › Storage | decision 0083 |
| ada7c30 | PLA-172 | A Play whose prerequisites ran restarts the app and starts the game fresh (JIT pool 232 → 20 MiB) | decision 0084; evidence 2026-10-11-prerequisites-before-a-restart |
| 24e2628, 1a2bb3a | PLA-154, PLA-170 | i386 .NET runs (madeira-unix 0112 COM+ image flags, 0113 atomic-store fault direction); .NET Framework dirs + WoW64 InstallRoot (wine-pe 0036) | decision 0085; evidence 2026-10-11-i386-dotnet |
| 715616e | PLA-174 | x64 exe fixed at 0x100000000 cannot load on iOS (PIE page zero + slide): recorded, not fixed; `__PAGEZERO` experiment left to the owner (PLA-176) | evidence 2026-10-11-fixed-base-4g |
| d64b28d | PLA-175 | 32-bit registry view audit: no change; `REQUIRED_I386` test; hazard PLA-177 | evidence 2026-10-11-wow64-registry-view |
| 2dff9fe, 56daa55 | PLA-174, PLA-170, PLA-158 | Steam's DirectX DXSETUP skipped (Proton builtin-copy parity): HasRunKey written, recorded `skipped`, depot 228990 not downloaded, no restart | decision 0086; evidence 2026-10-11-dxsetup-skipped |

Latest IPA on the phone: `b392cd08e5a4fed51d7d3dfa9f90feaefc54e90dcc760978f410b1dfcac4f38e`
(2dff9fe tree). Hollow Knight and Portal 2 reach their first frame on it.

## Running now

- **Zink chunks 1a then 1b** ([PLA-162](https://linear.app/playportdev/issue/PLA-162), plan
  [2026-10-11-opengl-zink](2026-10-11-opengl-zink.md)): writer started 2026-10-11 at 164d44b,
  run id in the build-area queue (`.work/agent-notes/speedrun/queue.md`), report
  `.work/agent-notes/speedrun/reports/zink-1.md`. 1a: Mesa GL + Zink + EGL for iOS as a framework,
  host test on the mock, notices, sizes, HK first-frame+10. 1b: the x86-64 `gl21` probe draws a
  clear colour and two triangles on the phone through `winios_OpenGLInit`; decision record.
  Each sub-chunk is committed on its own; a new session checks `git log` for "1a"/"1b" commits.

Done just before: DirectX setup skip (2dff9fe, 56daa55, decision 0086; report
`reports/dxsetup-skip.md`; IPA `b392cd08…`, Castle Crashers Demo first frame +4.4 s with no
prerequisite run, HK +9.0 s, P2 +6.1 s; Linear PLA-174/170/158 updated).

## Queue (in order; owner-set 2026-10-11)

1. **OpenGL via Zink, [PLA-162](https://linear.app/playportdev/issue/PLA-162), the rest:** chunk by
   chunk per [2026-10-11-opengl-zink](2026-10-11-opengl-zink.md) after 1a/1b (running): 2 WGL with
   DDNet/Teeworlds, 3 i386 with the measurement gate, 4 GoldSrc (Counter-Strike, Half-Life), 5 docs.
   Update that plan's status after each chunk as well as this one.
2. **[PLA-115](https://linear.app/playportdev/issue/PLA-115):** FEX with SMC detection off after
   Mono's backpatcher calls a null target. Fix the root cause so Duck Paradox's
   `MADEIRA_KEEP_SMC=1` compat default can be removed.
3. **[PLA-161](https://linear.app/playportdev/issue/PLA-161):** Vulkan (DXVK/KosmicKrisp) draws
   Unity 6 URP 3D scenes washed out. Fix it so Cat Mail Co. Demo's `graphics: dxmt` default can go.
4. **Phase 2, the library A–Z** (resumes here; #1 #DRIVE Rally is done, next is **9 Years of
   Shadows**, GOG 2099051765): the triage in the build area
   (`.work/agent-notes/speedrun/library-triage.md`: ProtonDB, anti-cheat, API, size, verdict per
   title) and the PLA-163 import census (`.work/agent-notes/pla163/census.md`). Skip SKIP
   verdicts, empty depots, dead services, and a second store copy once one is tested. One
   per-game worker per title (brief above); the supervisor creates the Linear issue from the report.
5. **Phase 1 rest: DiRT 3 Complete Edition** (Local import, 15 GB, i386 D3D9,
   [PLA-24](https://linear.app/playportdev/issue/PLA-24)); needs ~17 GB free, so uninstall House
   of Golf 2 (21.9 GB) for it (rule above); its own OpenAL installscript now runs (PLA-158).

Not yet scheduled (pick up after the queue or when a title needs them):
[PLA-177](https://linear.app/playportdev/issue/PLA-177) (a `Software\Wow6432Node` key hides
HKLM\Software from i386 callers; supervisor lean: a server fallback patch),
an XNA title to first frame + 30 s (Mono's SMC cost under i386 FEX),
[PLA-164](https://linear.app/playportdev/issue/PLA-164) (WoW64 host fault; Counter-Strike; retest
after the 0084 restart), [PLA-173](https://linear.app/playportdev/issue/PLA-173) (winedbg for
arm64ec), [PLA-99](https://linear.app/playportdev/issue/PLA-99) (SteamStub 3.1 x86),
[PLA-166](https://linear.app/playportdev/issue/PLA-166), [PLA-167](https://linear.app/playportdev/issue/PLA-167),
[PLA-168](https://linear.app/playportdev/issue/PLA-168), [PLA-157](https://linear.app/playportdev/issue/PLA-157).

Owner decisions open: [PLA-155](https://linear.app/playportdev/issue/PLA-155) (Wine Mono size),
[PLA-169](https://linear.app/playportdev/issue/PLA-169) (per-title prefixes),
[PLA-176](https://linear.app/playportdev/issue/PLA-176) (`__PAGEZERO` experiment). The owner signs
off the release app's launcher/dialog behaviour (decision 0068) before a merge.

## Phone state (2026-10-11 13:00)

Unattended mode on. Installed: Hollow Knight, Portal 2, House of Golf 2 (21.9 GB). Everything else
tested was uninstalled through the UI; `Games/Steamworks Shared` may hold 228988 (VC++ 2019,
23 MB). About 14.5 GB free; check `pp phone status` before any install. The prefix records
Castle Crashers' DXSETUP as failed (0xc0000018, build 984d70b9…), Counter-Strike's VC++ 2019
x86 as 1638 (downgrade refused: HoG2 installed 14.38) and x64 as success, and the VC++ 2022
HasRunKeys. Owner files in Documents (e.g. `Playport-PLA-147.ipa`) stay.

## Where things are

- Build-area queue (kept in step with this plan, which wins): `.work/agent-notes/speedrun/queue.md`.
- Per-game and per-chunk reports: `.work/agent-notes/speedrun/reports/` (copies of each child's
  report; `p2-NN-<slug>.md` for Phase 2 titles).
- Brief: `.work/agent-notes/speedrun/brief.md`. Library triage:
  `.work/agent-notes/speedrun/library-triage.md`. Census and prefix diff: `.work/agent-notes/pla163/`.

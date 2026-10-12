# Plan: compatibility speedrun

**Date:** 2026-10-08 (started); paused 2026-10-09; resumed 2026-10-10/11. **Kind:** execution
plan, in progress. **Linear:** [PLA-112](https://linear.app/playportdev/issue/PLA-112) (umbrella;
each game has its own Compatibility issue, each cross-title fault its own issue).

**Branch:** `dev/pla-152-gdi-windows` (from `main` 6ad89e8), unmerged and unpushed; it carries
all of the speedrun's work since 2026-10-10. (`dev/compat-speedrun`, the first leg, is merged
into `main`.) Merge to `main` with `--no-ff` only when the owner asks. Last updated
2026-10-12 13:00 at 86b00d5 (+ this plan commit).

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
| Football Manager 2022 (Epic) | PLA-114 | Broken: small window at +6 s, no frame in 180 s; anti-tamper crash gone after wine-pe 0038 | 1c20a0c, af4be23, 9d40a49 |
| Monster Train (GOG) | PLA-54 | Works | 295e18a |
| Soccer Online: Ball 3D | PLA-53 | Works | f7bd7a1 |
| Duck Paradox (GOG) | PLA-52 | Works (compat default `MADEIRA_KEEP_SMC=1`, PLA-115) | 7c2a71f |
| Jurassic World Evolution (Epic) | PLA-41 | Broken: DRM wild store (PLA-122) | ca7289c |
| Among Us | PLA-39 | Works | ed9191f |
| Mortal Kombat Komplete Edition (Local) | PLA-36 | Playable with issues: defaults, menus to a pad fight at ~53 FPS | 380c5c0, 0c51652, c3cda59, 60d6de9 (fex 0025) |
| Tomb Raider (203160) | PLA-150 | Playable with issues: launcher shown, Play → main menu | PLA-152/151 |
| Castle Crashers Demo (207100) | PLA-159 | Works (Vulkan, i386 D3D9); full game PLA-34 likely blocked by SteamStub x86 (PLA-99) | none |
| Cat Mail Co. Demo (4622530) | PLA-160 | Works on Vulkan (default): tutorial drawn right at 60 FPS | fee230f (NSI UDP table), 246365a (mesa 0030, PLA-161) |
| Neverwinter Nights: EE (GOG 1097893768) | PLA-185 | Playable with issues: gameplay through OpenGL/Zink at 50 fps; fullscreen needs a `settings.tml` edit | 22e2abc |
| Baldur's Gate: EE (GOG 1207666353) | PLA-183 | Playable with issues: main menu on OpenGL (Zink) after GOG's OpenAL redist; gameplay and sound unchecked | ad2fce2 (PLA-182, 0090) |
| Neverwinter Nights Diamond (GOG 1207658890) | PLA-187 | Broken: `binkw32.dll` not found | — |
| Quake II (GOG 1441704824) | PLA-188 | Not installable from GOG in the app | — |
| Counter-Strike (10) | PLA-22 | Playable with issues: OpenGL (`-gl`) de_dust2 at 59.7 FPS, no input tested; `-soft` broken (PLA-164) | e5a928b (PLA-9), PLA-162, 9a2906c |
| Half-Life (70) | PLA-25 | Playable with issues: OpenGL c1a0 at 60 FPS, GPU 12–15 ms (PLA-191); can't share its folder with CS (PLA-192) | PLA-162, 9a2906c |

Compat defaults in `app/PlayportKit/Titles/compat-defaults.json`: Super Meat Boy Epic `aspect: 16:9`, Duck Paradox `env.MADEIRA_KEEP_SMC=1` (PLA-115).

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
| 69db234, 8e5eabd | PLA-162 | OpenGL through Zink over KosmicKrisp (x86-64): Mesa EGL/Zink frameworks, `winios_OpenGLInit`, GL 3.3 | decision 0087; evidence 2026-10-11-mesa-gl-ios-build, 2026-10-11-opengl-zink-route |
| 9a2906c, e77d8b9 | PLA-162, PLA-22, PLA-25 | GoldSrc on OpenGL (Counter-Strike, Half-Life to a map at 60 FPS); madeira-unix 0117 moves a code-free i386 image without relocations; OpenGL in ARCHITECTURE/BUILDING; Zink plan finished | evidence 2026-10-11-opengl-goldsrc; finished.md |
| be3a61a, aa4549f, 3b334b5 | PLA-162 | OpenGL for i386 through WoW64 (generator conversion through the window); host-side Zink passes the i386 budget | decision 0089; evidence 2026-10-11-opengl-i386 |
| 22e2abc, 28308e7 | PLA-162, PLA-179, PLA-180 | OpenGL WGL completeness (x86-64): surface at first make-current, dummy-window hand-off, sRGB, no crash without EGL; frame limit through `PacedMetalLayer` | decision 0088; evidence 2026-10-11-opengl-wgl-completeness |
| 2dff9fe, 56daa55 | PLA-174, PLA-170, PLA-158 | Steam's DirectX DXSETUP skipped (Proton builtin-copy parity): HasRunKey written, recorded `skipped`, depot 228990 not downloaded, no restart | decision 0086; evidence 2026-10-11-dxsetup-skipped |

Latest IPA on the phone: `e89e0ba68063b63fa9b3c16e69fc8b52a6e6237b9799a4a4899ce9c8e4f32463`
(68fa619: FEX CPUID brand string, killed thread ends at a safe point). Hollow Knight and Portal 2 reach their first frame on it.

## Running now

- **Issue sweep 2.10c, FEX thread faults:** [PLA-224](https://linear.app/playportdev/issue/PLA-224) an x86-64 thread created after another exits
  faults in FEX's dispatcher at its first syscall stub (first; High), then [PLA-223](https://linear.app/playportdev/issue/PLA-223) a busy
  TerminateThread victim faults in libwow64fex and keeps the FEX lock. Writer started 2026-10-12 from
  the plan commit after 86b00d5. Run id in `.work/agent-notes/speedrun/queue.md`; report
  `.work/agent-notes/speedrun/reports/sweep-10c-fex-threads.md`. Testing budget (owner, 2026-10-12):
  probe runs that show each fix, then one HK + P2 first-frame+10 on the final IPA; no P2 perf.

Done before (newest first):
- **Done** (68fa619, and the docs commit after it). **Issue sweep 2.10b, FEX CPUID and crash handling:** [PLA-220](https://linear.app/playportdev/issue/PLA-220) CPUID brand-string leaves read
  `PerCPUData` past its end, [PLA-173](https://linear.app/playportdev/issue/PLA-173) winedbg for arm64ec, [PLA-200](https://linear.app/playportdev/issue/PLA-200) asynchronous abort of a
  killed thread:
  - **PLA-220 fixed:** fex 0026 keeps the core index inside the per-CPU data, and the iOS MIDR names
    "Apple Silicon" (decision 0098). i386 probe `done ok` on 6 threads; x86-64 reads "Apple Silicon"
    on every core it reached.
  - **PLA-173, no change:** a pseudo-process cannot be debugged (no task port, so
    `NtDebugActiveProcess` returns `c0000022`); i386's staged winedbg exits and the crashed child
    ends in 0.7 s; x86-64 has none, as before.
  - **PLA-200 in part:** madeira-unix 0127 diverts a killed thread to `user_mode_abort_thread` in
    translated code; 8 busy workers of crashed children ended. The `terminate` phase still hangs
    on a FEX fault in a victim, which predates the change.
  - **Found:** an x86-64 thread on a reused TEB faults at its first syscall stub (follow-up).
  - IPA `e89e0ba6…`: HK first frame +9.39 s, P2 +6.27 s. Evidence 2026-10-12-fex-cpuid-crash-handling;
    report `.work/agent-notes/speedrun/reports/sweep-10b-fex-crash.md`.
  Linear: PLA-220 In Review; PLA-173 evidence (blocked by [PLA-221](https://linear.app/playportdev/issue/PLA-221), Low); PLA-200 partial. Follow-ups [PLA-224](https://linear.app/playportdev/issue/PLA-224) (High, x86-64 thread fault at first syscall), [PLA-223](https://linear.app/playportdev/issue/PLA-223) (WoW64 TEB+0x1788 fault holding the FEX lock), [PLA-222](https://linear.app/playportdev/issue/PLA-222) (killed thread in a wait), inserted as 10c/10d.
- **Done** (74f1212, and the docs commit after it). **Issue sweep 2.10a, prefix and loader leftovers:** [PLA-167](https://linear.app/playportdev/issue/PLA-167) `xaudio2_9redist.dll`, [PLA-168](https://linear.app/playportdev/issue/PLA-168) wineboot
  machine values, [PLA-157](https://linear.app/playportdev/issue/PLA-157) win32u unix init per process, [PLA-166](https://linear.app/playportdev/issue/PLA-166) typelibs / CurrentControlSet:
  - **PLA-168 fixed:** the host writes wineboot's processor values at every session start (decision 0098):
    `NUMBER_OF_PROCESSORS` 6, `PROCESSOR_*`, `HARDWARE\DESCRIPTION\System` and a CentralProcessor key a
    processor; the seed's Volatile Environment has `USERDOMAIN`/`LOGONSERVER`. Machine probe `done ok`
    (x86-64 and i386).
  - **PLA-157 fixed:** wine-unix 0032 makes win32u's thread info key once; a second process no longer
    empties the running processes' thread info.
  - **PLA-167, no change:** neither Wine nor Proton has `xaudio2_9redist.dll`; the phone matches Proton,
    and a title's own copy loads (LoadLibrary and import).
  - **PLA-166, evidence only:** `CurrentControlSet` is a plain key (the seed exists before the server's
    first load); only DXMT's unused NVEXT touches `ControlSet001`; stdole2 loads.
  - **Found:** FEX's CPUID brand string reads past its one-CPU table on iOS (garbage); follow-up.
  - IPA `2bec1330…`: HK first frame +10.35 s, P2 +6.11 s; P2 perf 60 FPS, 15.2 GPU ms, 60.2 M
    instructions a frame (unchanged). Evidence 2026-10-12-prefix-loader-leftovers; report
    `.work/agent-notes/speedrun/reports/sweep-10a-prefix-loader.md`. Item 10b ([PLA-173](https://linear.app/playportdev/issue/PLA-173) winedbg, [PLA-200](https://linear.app/playportdev/issue/PLA-200)
    async abort) follows.
  Linear: PLA-168/157 In Review, PLA-167 closed as Proton parity, PLA-166 evidence (Low). Follow-up [PLA-220](https://linear.app/playportdev/issue/PLA-220) (FEX CPUID brand string read out of bounds), High, inserted as 10b.
- **Done** (86d91c1, and the docs commit after it). **Issue sweep 2.9, [PLA-186](https://linear.app/playportdev/issue/PLA-186) in-game menu Screenshot says "No frame to save" for KosmicKrisp-presented
  titles (GL and DXVK):**
  - **Cause:** KosmicKrisp's layer is framebuffer-only, so the drawable cannot be read.
  - **Fix:** mesa 0035 hangs the present blit's source image on the drawable texture. The menu
    reads that image, through a 2D blit copy made only when Screenshot is chosen. The layer stays
    framebuffer-only, and no work is added per frame.
  - **Phone:** on IPA `0521958c…`, saved and not black on P2 (DXVK), HK (DXVK on this phone),
    `wgl` (Zink) and Death's Door (DXMT, read directly). HK first frame +10.5 s, P2 +6.3 s.
  - **Perf:** P2 60 FPS, 15.18 GPU ms and 60.1 M instructions a frame (unchanged).
  - **Photos prompt:** a screenshot on a phone without the Photos add-only permission raises the
    iOS prompt, which blocks driven runs until a person answers.
  - Evidence 2026-10-12-screenshot-kosmickrisp; report
    `.work/agent-notes/speedrun/reports/sweep-09-screenshot.md`.
  Follow-up [PLA-219](https://linear.app/playportdev/issue/PLA-219) (the Photos add-only prompt blocks driven runs; menu-quit exit 0x42b), Low. Owner tapped Allow on 2026-10-12.
- **Done** (6fd94a1, 597d663, and the docs commit after them). **Issue sweep 2.8b, [PLA-197](https://linear.app/playportdev/issue/PLA-197) iOS display source and mode emulation: a named
  `ChangeDisplaySettings` fails (GoldSrc `-soft`, any mode-changing game):** decision 0097 (amends
  0088). The virtual monitor lists Wine's emulated modes, and a listed mode becomes current
  (madeira-unix 0125). Zink acquires a window buffer before a copy reads it (mesa 0034). A GL swap
  keeps the layer's drawable at the window's size (madeira-unix 0126). On IPA `8221f31b…`,
  CS `-soft -full` reaches de_dust2 at first frame +30 s, full screen 4:3 with bars at the sides.
  CS `-gl`, HK and P2 first frame are unchanged. P2 perf: 60 FPS, 15.18 GPU ms, 60.2 M
  instructions a frame (unchanged). Open: what resets the GL layer's drawable size, and CS's
  intermittent start-up stall after its intro. Evidence 2026-10-12-display-modes; report
  `.work/agent-notes/speedrun/reports/sweep-08b-display-modes.md`.
  Follow-ups [PLA-217](https://linear.app/playportdev/issue/PLA-217) (GL layer drawableSize reset after a resize) and [PLA-218](https://linear.app/playportdev/issue/PLA-218) (CS `-soft` intermittent start-up stall; windowed `-soft`), both Low.
- **Done** (825c102, and the docs commit after it). **Issue sweep 2.8a2b, [PLA-212](https://linear.app/playportdev/issue/PLA-212) run the installer named by Steam's standard VC++ `.cmd` directly
  (owner approved 2026-10-12: "outcome on par with Steam/Proton, not the exact process"):** decision 0096
  (outcome parity, amends 0081/0082), [evidence](../evidence/2026-10-12-vcredist-direct.md), IPA `b74365c0…`.
  - **Now:** PlayportKit `SteamVCRedistCmd` matches only `start /w "" "%~dp0\<vc_redist…>.exe" <words>`
    (plus `@echo off`, comments, the 3010 and `exit /b` tails); that installer runs directly with the same
    arguments and folder, logged `prerequisite: direct (standard VC++ .cmd) …`; anything else through `cmd`.
  - **Phone:** Counter-Strike's 2019 x86 rerun (0x666 = 1638, a failure per 0083, unchanged) ran Burn and
    its `.cr` only, no console, head +13.9 MiB (was 99.2), room 462 MiB, game in the same session
    (`20261012T093429`); HK +10.03 s, P2 +5.77 s.
  - **Open:** a fresh x86+x64 pair could not be had through the UI (every VC++ HasRunKey already in the
    prefix); computed about 36 MiB of head, about 439 MiB of room, so no restart. A real first install run
    directly is not yet observed.
  Follow-up [PLA-215](https://linear.app/playportdev/issue/PLA-215) (observe a real first VC++ install run directly; Low). [PLA-213](https://linear.app/playportdev/issue/PLA-213) lowered and moved to 14b.
- **Done** (239283e, and the docs commit after it). **Issue sweep 2.8a2, [PLA-208](https://linear.app/playportdev/issue/PLA-208) launch screen: first-run dependency installs
  (owner request 2026-10-12):** decision 0095,
  [evidence](../evidence/2026-10-12-launch-screen-dependencies.md), IPA `1ebae087…`.
  - **Cause:** a Steam `.cmd` entry's console window (the first guest window) mapped the stage to the
    game's ("Starting the game…") 0.4 s into the install. No other first-run work runs under the launch
    screen long enough to need a text (DXSETUP skipped, Mono no install step, seed and links in ms).
  - **Now:** "Installing dependencies (1 of 2)…", its seconds past 10 s, a Dependencies dot from the
    first install on (three dots otherwise), each install its share of the bar, the installer's window
    keeps the stage, restart screen "Dependencies installed. Starting <title>…", a failure waiting on
    an install is that step's.
  - **Phone:** Counter-Strike's rerun shows the text and four dots (`20261012T091403`); forced restart
    on Jurassic World Evolution shows the new wording (`20261012T091522`); HK +9.61 s, P2 +6.41 s,
    three dots.
  - **Open:** whether an installer's window should end the launch screen (0075) is left to the owner.
  Follow-up [PLA-214](https://linear.app/playportdev/issue/PLA-214) (a prerequisite's own window ends the launch screen; Low).
- **Done** (a096230, 7788296). **Issue sweep 2.8a, [PLA-209](https://linear.app/playportdev/issue/PLA-209) avoid the Playport restart between first-run prerequisites and the
  game (decision 0084; owner request 2026-10-12):**
  - **Measured:** a Steam VC++ `.cmd` entry takes about 100 MiB of head, 84 of it `cmd`, `conhost`
    and `start`. An installer run directly takes 7 to 14 MiB.
  - **Chosen:** option (3), decision 0094. The restart happens only when the prerequisites left
    the game less than 320 MiB.
  - **On the phone** (IPA `c8e5ffaa98e69806a8fee4667a3242c9092c62651ca537b4608995f0ec2b3918`):
    - Counter-Strike's VC++ x86 entry ran and the game started 0.27 s later, without a restart.
    - A forced short pool still restarts.
    - HK and P2 reach their first frame.
  - **Still open:** a fresh VC++ x86+x64 pair still restarts. The follow-ups are returning dead
    image ranges to the head, or running Steam's standard VC++ `.cmd` installer directly.
  - **Records:** [evidence](../evidence/2026-10-12-prereq-no-restart.md); report
    `.work/agent-notes/speedrun/reports/sweep-08a-prereq-restart.md`.
  Follow-ups [PLA-213](https://linear.app/playportdev/issue/PLA-213) (runtime: return dead image ranges to the pool head; ends the fresh VC++ pair restart; sweep 8a3) and [PLA-212](https://linear.app/playportdev/issue/PLA-212) (run the standard VC++ `.cmd`'s installer directly; owner decision).
- Sweep 2.8, Zink follow-ups: [PLA-198](https://linear.app/playportdev/issue/PLA-198) windowed GL GoldSrc hang, [PLA-184](https://linear.app/playportdev/issue/PLA-184) MSAA context rebound
  crash, [PLA-181](https://linear.app/playportdev/issue/PLA-181) custom border colour, [PLA-189](https://linear.app/playportdev/issue/PLA-189) i386 persistent maps, [PLA-191](https://linear.app/playportdev/issue/PLA-191) Half-Life GPU time.
  Writer started 2026-10-12 from the plan commit after ccb48b3. Run id in
  `.work/agent-notes/speedrun/queue.md`; report `.work/agent-notes/speedrun/reports/sweep-08-zink-followups.md`.
  Done = each issue fixed or with evidence and a next step; HK/P2 first frame.
  **Done (2026-10-12):** d506e4a (mesa 0031: Zink acquires a swap-chain resolve target before the
  batch uses it; PLA-198 and PLA-184 were one fault, both fixed) + 81ab29b (mesa 0032: custom border
  colours for Zink only, PLA-181; mesa 0033: no triangle fans on KosmicKrisp, PLA-191: Half-Life c1a0
  1.59 GPU ms from 15.28, CS de_dust2 1.57 from 3.08; decision 0093; PLA-189 not implemented, design
  and next step in the evidence) + this entry; evidence 2026-10-12-zink-followups. IPA `673566a7…`:
  HK, P2, CS `-gl -full` first frame; P2 perf unchanged (15.20 GPU ms, 60.8 M instr/frame); CS back
  in full screen; HL installed, profiled and uninstalled.
  Follow-ups [PLA-210](https://linear.app/playportdev/issue/PLA-210) (GL layer lock held across the swap) and [PLA-211](https://linear.app/playportdev/issue/PLA-211) (`pp gpu capture` leaves .gputrace bundles in the game folder), both Low.
- Sweep 2.7, [PLA-192](https://linear.app/playportdev/issue/PLA-192) Steam apps sharing an install folder (Counter-Strike 10 / Half-Life
  70), with [PLA-123](https://linear.app/playportdev/issue/PLA-123) a compat default lost when the player types their own Runtime keys: done
  (2026-10-12): ee2b96e (decision 0092: a folder another Steam record names is joined file by file,
  only files no other app lists are downloaded, a file another app lists with other content refuses
  the install, uninstall removes only the app's own files; decision 0091 amends 0074: Runtime keys and
  Graphics options merge with a compat default by key, Arguments unchanged) + 8fa6792 (the game page
  shows a default's keys) + the evidence commit (2026-10-12-shared-installdir-compat-merge), IPA
  `88613c10…`. HL installed beside CS (124 of 4488 files downloaded), HL c1a0 and CS de_dust2 first
  frame, HL uninstalled through the UI: CS verifies 5611/5611 and reaches de_dust2. HL's AccessDenied
  ticket is expected (no licence for app 70; its depots come through another package). Duck Paradox
  with `env.FOO=1` keeps `MADEIRA_KEEP_SMC=1` and reaches its menu (installed, tested, uninstalled).
  HK +10.19 s, P2 +6.33 s. CS kept installed. Report
  `.work/agent-notes/speedrun/reports/sweep-07-shared-dir-compat.md`. Follow-up: an update of a shared
  depot is refused while the other app has the old content (PLA-192 follow-up).
  Follow-up [PLA-207](https://linear.app/playportdev/issue/PLA-207) (shared-depot updates across apps in one folder; Low, not in the sweep order).
- Sweep 2.6c, [PLA-205](https://linear.app/playportdev/issue/PLA-205) stale anon JIT alias after `MEM_RELEASE`, with [PLA-201](https://linear.app/playportdev/issue/PLA-201) (re-check
  FlushInstructionCache staleness after wine-pe 0038): done (2026-10-12): e4d8aa6
  (`patches/madeira-unix` 0124: `free_pages` retires, or trims, the anon aliases a
  `MEM_RELEASE` frees; `MEM_DECOMMIT` keeps them; `patches/wine-pe` 0039: the i386
  `FlushInstructionCache` calls `NtFlushInstructionCache`, so FEX translates the range again)
  + the evidence commit (2026-10-12-jit-alias-release), IPA `b1f0278b…`. Probe `exec-rw-reuse`
  passes. PLA-201 was not fixed by 0038 (87.5 % stale): it is now 0 stale. HK +10.15 s and P2
  +6.17 s first frame. P2 perf unchanged (60 FPS, 15.19 GPU ms, 58.9 M instr/frame). Duck
  Paradox default +6.07 s; with `MADEIRA_KEEP_SMC=0` unchanged (installed, tested,
  uninstalled). FM2022 unchanged (window +6.48 s, no first frame in 180 s). Follow-up: FEX's
  WoW64 write trap misses a 4 KiB page sharing a 16 KiB host page with writable pages. Report
  `.work/agent-notes/speedrun/reports/sweep-06c-jit-alias.md`.
  Follow-up [PLA-206](https://linear.app/playportdev/issue/PLA-206) (WoW64 SMC trap misses a 4 KiB code page in a shared 16 KiB host page; with item 14).
- Sweep 2.6b, [PLA-203](https://linear.app/playportdev/issue/PLA-203) FEX `NoExec` for code written to a `PAGE_READWRITE` allocation then
  made `PAGE_EXECUTE_READ`: done (2026-10-12): 9d40a49 (`patches/wine-pe` 0038: the ARM64EC
  `NtProtectVirtualMemory` `[exec-req]` diagnostic returned without `leave_syscall_callback()`,
  so after each process's first execute protect FEX got no alloc/protect/free/map notifications
  on that thread) + the evidence commit (2026-10-12-noexec-rw-rx; probe `exec-rw` now first,
  `exec-rw-reuse` added), IPA `ad449e16…`. `exec-rw` passes on the phone; HK +9.62 s and P2 +5.53 s
  first frame; P2 perf unchanged (60 FPS, 15.19 GPU ms, 58.6 M instr/frame). FM2022 no longer
  dies at 5 s: a 550×146 window at +6.16 s, no first frame in 180 s. JWE unchanged (`0x1488a3cf6`).
  Duck Paradox without `MADEIRA_KEEP_SMC` unchanged (installed, tested, uninstalled). Follow-up:
  anon JIT aliases outlive `MEM_RELEASE` (probe `exec-rw-reuse`). Report
  `.work/agent-notes/speedrun/reports/sweep-06b-noexec-rw-rx.md`.
  Follow-up [PLA-205](https://linear.app/playportdev/issue/PLA-205) (stale anon JIT alias after `MEM_RELEASE`; next with PLA-201, as 2.6c).
- Sweep 2.6, [PLA-118](https://linear.app/playportdev/issue/PLA-118) per-page NtProtect RX flips each pay a full JIT-mapping scan
  (Football Manager 2022, [PLA-114](https://linear.app/playportdev/issue/PLA-114)), then [PLA-122](https://linear.app/playportdev/issue/PLA-122) the Codefusion DRM wild store (Jurassic World
  Evolution, [PLA-41](https://linear.app/playportdev/issue/PLA-41)) if time allows: done (2026-10-12): af4be23 (`patches/madeira-unix`
  0122: LDAPUR/STLUR in a sub-floor window; probe `tools/compat/probes/protect.c`), 2b1501e (0123,
  diagnostics) + the evidence commit (2026-10-12-ntprotect-sync), IPA `57d12a62…`. The protects
  were not the blocker (about 1 to 2 s for all of them; 12 to 15 µs a one-page protect on the
  phone), so the sync was left as it is. FM2022 looped in a user shared data read FEX lowers to
  LDAPUR, which the sub-floor service did not know; with 0122 it decrypts and then dies
  `c0000005` at 5 s like JWE. Both deaths are deterministic at one RIP and independent of TSO,
  and the unaligned-atomic round trip keeps every register (0123), so PLA-122 stays open (a
  probable tamper response). HK and Portal 2 gates ok. JWE reinstalled (24.5 GB free). Report
  `.work/agent-notes/speedrun/reports/sweep-06-ntprotect.md`.
  Follow-ups [PLA-203](https://linear.app/playportdev/issue/PLA-203) (FEX NoExec after RW→RX; next, as 2.6b) and [PLA-204](https://linear.app/playportdev/issue/PLA-204) (USD read cost; low).
  PLA-122 stays open (anti-tamper check trace; needs an Epic token on the workstation): back in the
  sweep after the titles (item 11).
- Sweep 2.5c, [PLA-202](https://linear.app/playportdev/issue/PLA-202) FEX `X87ReducedPrecision` rounds MSVC's `fild`/`fistp` m64 atomic
  load (MKKE crashes at 69 s): done (2026-10-12): 60d6de9 (`patches/fex` 0025: the x87 stack
  pass keeps a `fild m64` integer for a `fist m64` of the unchanged value; probe
  `tools/compat/probes/x87m64.c`) + the evidence commit (2026-10-12-x87-fild-fistp-m64), IPA
  `3e81f622…`. The probe is exact at reduced precision; MKKE with defaults ran past 69 s, and a
  pad play reached a fight (Scorpion against Noob, 53 FPS): playable with issues, then uninstalled
  (33.0 GB free). HK and Portal 2 gates ok; Portal 2 perf unchanged (60 FPS, 60.2 → 62.6 M
  instructions a frame, one run each). Report
  `.work/agent-notes/speedrun/reports/sweep-05c-x87-m64.md`.
  MKKE set to Playable with issues (Linear PLA-36, site).
- Sweep 2.5b, [PLA-199](https://linear.app/playportdev/issue/PLA-199) Local import space check counts a same-volume clone as a full copy,
  then the Mortal Kombat Komplete Edition ([PLA-36](https://linear.app/playportdev/issue/PLA-36)) replay: done (2026-10-12): c3cda59 (a
  same-volume clone is checked for the reserve, a failed clone before its copy) + the evidence commit
  (2026-10-12-mkke-import-x87), IPA `34055093…`. MKKE imported with 13.8 GB free and reached its first
  frame at +5.3 s, dxdiag exiting 0; it then crashes at about 69 s because FEX's 64-bit x87 rounds its
  `fild`/`fistp` atomic 64-bit load. With 80-bit x87 the attract-mode fight runs. It was uninstalled
  through the UI (33.0 GB free). Next: how a Local title gets a profile entry for 80-bit x87. Report
  `.work/agent-notes/speedrun/reports/sweep-05b-import-mkke.md`.
  Follow-up [PLA-202](https://linear.app/playportdev/issue/PLA-202) (FEX reduced precision rounds `fild`/`fistp` m64; next, as 2.5c).
- Sweep 2.5, [PLA-120](https://linear.app/playportdev/issue/PLA-120) rpmalloc lock deadlock after a WoW64 child aborts, with [PLA-117](https://linear.app/playportdev/issue/PLA-117)
  Wine dxdiag failing (Mortal Kombat Komplete Edition, [PLA-36](https://linear.app/playportdev/issue/PLA-36)), done (2026-10-12):
  0c51652 (prefix seed names the computer; madeira-unix 0120 diagnostics, 0121; wine-unix 0031;
  probe `tools/compat/probes/childabort.c`) + the evidence commit (2026-10-12-dxdiag-child-abort).
  dxdiag failed because `GetComputerNameW` had no `ActiveComputerName` (the app runs no wineboot);
  it now writes its report on the phone. The allocator lock was a victim: a starting child's
  `virtual_alloc_teb` and a WoW64 `NtProtectVirtualMemory` deadlocked on `virtual_mutex` and the
  WoW64 window registry's lock, and a parent thread waiting for the registry inside FEX's
  allocator left the others spinning in `fex_ios_rpm_lock`; 0121 fixes the order and gives the
  registry lock back at a thread's abort. 0031 stops the lock monitor's read of a dead child's page
  ending the app. IPA `2d99e298…`: probe `done ok`, HK and P2 ok. Mortal Kombat not played: its
  import's space check wanted 20.5 GB with 15.8 GB free after House of Golf 2 was uninstalled
  (not reinstalled). Report `.work/agent-notes/speedrun/reports/sweep-05-dxdiag-rpm.md`. Next:
  follow-ups for asynchronous thread abort, in-place code rewrites under FEX, and the import's
  space check for a clone; then Mortal Kombat.
  Follow-ups [PLA-199](https://linear.app/playportdev/issue/PLA-199) (import space check; next, as 2.5b), [PLA-200](https://linear.app/playportdev/issue/PLA-200) (async abort; with item 10),
  [PLA-201](https://linear.app/playportdev/issue/PLA-201) (FEX FlushInstructionCache; before PLA-115, as 13b).
- Sweep 2.4, [PLA-164](https://linear.app/playportdev/issue/PLA-164) GoldSrc `-soft` host fault, done (2026-10-12):
  836a072 (madeira-unix 0118) + 86899eb (wine-pe 0037, host test `dc_from_memory_test.c`) +
  4bec238 (madeira-unix 0119) + the evidence commit (2026-10-12-goldsrc-soft-host-fault). There
  was no stack-end fault. `-soft` hit an unaudited `D3DKMTCreateDCFromMemory` thunk (`c00000bb`;
  0037 audits it). A thread of the ended game then faulted in its reclaimed pool copies and stormed
  until the app died (0118 ends such a thread). Now the game's named `ChangeDisplaySettings` fails
  (no display source on iOS). Its fallback unloads SDL2 and calls the dead window procedure, and
  that storm ends only the title (0119: `c0000005`, *Counter-Strike stopped unexpectedly*, app
  alive). IPA `7138d11e…`: HK, P2 and CS `-gl -full +map de_dust2` ok. Report
  `.work/agent-notes/speedrun/reports/sweep-04-goldsrc-soft.md`. Next: display modes for a named
  device (follow-up).
  Follow-ups [PLA-197](https://linear.app/playportdev/issue/PLA-197) (display source and mode emulation; added to the sweep as 8b) and [PLA-198](https://linear.app/playportdev/issue/PLA-198)
  (windowed GL GoldSrc host-mutex hang; added to the Zink follow-ups).
- Sweep 2.3, [PLA-177](https://linear.app/playportdev/issue/PLA-177) one `Software\Wow6432Node` key hides HKLM\Software from i386, done (2026-10-12):
  1498a09 (58534bb reverted; `patches/mesa` equals 246365a's) + 33b7cb1 (supervisor decision in
  2026-10-11-kk-ios27-workarounds) + 6748815 (wine-unix 0030) + the evidence commit
  (2026-10-12-wow6432node-fallback). On iOS, an i386 lookup of a name that a key under
  `HKLM\Software\Wow6432Node` lacks is looked up, or created, at the same path under
  `HKLM\Software` (`KEY_WOWSHARE` semantics, at every depth; keys only, not values); the server
  logs `[wow64-reg]` once. Desktop A/B (Wine 11.18, patched wineserver): the hazard's four
  `REGDB_E_CLASSNOTREG` are `S_OK`, the seed alone is unchanged. IPA `01bd8234…`: HK, P2 and
  Super Meat Boy (i386, XAudio2 2.7) first frame; no `[wow64-reg]` (the phone has no key). Fallback
  not exercised on the phone. Report `.work/agent-notes/speedrun/reports/sweep-03-wow6432node.md`.
  Follow-up [PLA-196](https://linear.app/playportdev/issue/PLA-196) (values, HKCU, on-phone proof; low).
- Sweep 2.2, [PLA-193](https://linear.app/playportdev/issue/PLA-193) KosmicKrisp on iOS 27, done (2026-10-11): 58534bb
  + 82b9825 (2026-10-11-kk-ios27-workarounds). Cat Mail needs only coherent descriptor
  set loads in its vertex shaders; `patches/mesa` 0030 now keeps workaround 6 for those alone
  (every stage but fragment). PLA-161's 11.7 ms control did not reproduce: workaround 6 off and the
  old 0030 both measure 15.2 ms in Portal 2, so there was no cost to win back; coherent descriptor
  loads in fragment shaders are the 2x cost. Survey of workarounds 1-5, 8, 11, 12, 17, 18 in the
  evidence: no symptom in any title we have. Cat Mail tutorial right, HK and P2 first frame on IPA
  `7bc21c14…`; Cat Mail uninstalled. Report `.work/agent-notes/speedrun/reports/sweep-02-kk-wa6.md`.
  **Supervisor decision:** the narrowing gained nothing measurable and covers fewer accesses, so
  58534bb is reverted to the broad 0030 (gated on 246365a) at the start of sweep 2.3. Follow-up
  [PLA-195](https://linear.app/playportdev/issue/PLA-195) (root cause of the stale descriptor loads).
- Sweep 2.1, [PLA-182](https://linear.app/playportdev/issue/PLA-182) OpenAL32 missing, done: 6169a33 + 
  ad2fce2 (GOG's build `dependencies` from GOG's dependency repository, run once per prefix from
  Play, decision 0090) and the evidence commit (2026-10-11-openal). Proton has no `OpenAL32.dll`;
  Heroic runs GOG's `__redist/openAL/oalinst.exe /S`, which Playport now runs too. BGEE (GOG
  1207666353, [PLA-183](https://linear.app/playportdev/issue/PLA-183)) reaches its main menu on
  OpenGL (Zink); HK and P2 first frame. IPA `a4174ddc…`; report
  `.work/agent-notes/speedrun/reports/sweep-01-openal.md`. No library shipped.
  Follow-up [PLA-194](https://linear.app/playportdev/issue/PLA-194) (GOG install script, v1 temp_executable, .msi redists; 0090 choices for owner review).
- [PLA-161](https://linear.app/playportdev/issue/PLA-161) Vulkan Unity 6 URP (Cat Mail Co. Demo): fixed, 246365a + 9e38373 (evidence
  2026-10-11-unity-urp-vulkan). KosmicKrisp's workaround 6 (coherent device accesses) was off on
  iOS 27 (the macOS-27 check); `patches/mesa` 0030 keeps it outside fragment shaders, 0029 adds the
  `MESA_KK_DEBUG=os26_workarounds` bisect switch. The `graphics: dxmt` default is gone; tutorial
  right at 60 FPS; HK (Vulkan and DXMT) and P2 first frame. Cost: Portal 2 GPU 11.7 → 15.2 ms a
  frame at 60 FPS: follow-up [PLA-193](https://linear.app/playportdev/issue/PLA-193). IPA `a1aa770a…`; report `reports/pla161-urp.md`.
- PLA-115 second attempt (3 h box), diagnosis only: 8e8def0 (evidence 2026-10-11-mono-backpatch-smc,
  "Second attempt"): no lost store in Wine's store emulator, no stale FEX block, no second copy, no
  concurrent JIT; Mono itself writes a mismatched patch list with tracking off. Hypothesis: FEX's
  SMC fault path inside Mono's patch loop. **Parked** after two attempts; Duck Paradox keeps
  `MADEIRA_KEEP_SMC=1`. Next steps in Linear PLA-115; diagnostic patches in
  `.work/agent-notes/speedrun/pla115-diag{,2}/`. Report `reports/pla115-smc-2.md`.
- PLA-115 first attempt, diagnosis only: eea80c8 (evidence 2026-10-11-mono-backpatch-smc): not a
  FEX link; the guest calls 0 from a Mono method whose `mov r11` templates in one page are still
  unpatched; FEX's SMC fault path and the RW plain view ruled out. Duck Paradox keeps its
  `MADEIRA_KEEP_SMC=1` default. Diagnostic FEX patches (uncommitted) in
  `.work/agent-notes/speedrun/pla115-diag/`. Report `reports/pla115-smc.md`.
- Zink chunks 4 and 5, GoldSrc and docs: ([PLA-162](https://linear.app/playportdev/issue/PLA-162),
  plan [2026-10-11-opengl-zink](finished.md#opengl-through-zink), now removed). 9a2906c: madeira-unix
  0117 (a code-free i386 image without relocations moves in the WoW64 window; Wine's `usp10.dll`
  had failed against the game's `Mss32.dll`), evidence 2026-10-11-opengl-goldsrc. Counter-Strike
  `-gl +map de_dust2` 59.7 FPS at the 60 limit, Half-Life `-gl +map c1a0` at 60 FPS; `-soft` still
  fails (PLA-164); Isaac and Geometry Dash are not owned. e77d8b9: ARCHITECTURE/BUILDING OpenGL,
  finished.md, plan removed. IPA `7c636f4f…`; report `reports/zink-4.md`.
  Linear: PLA-22/PLA-25 Playable with issues, PLA-162 In Review; follow-ups PLA-190 (owner licensing
  review of `MesaGL` notices), PLA-191 (Half-Life GPU 12–15 ms), PLA-192 (CS/HL shared install folder).
- Zink chunk 3, i386 GL through WoW64: be3a61a (decision 0089 budget set before measuring),
  aa4549f (wine-unix 0029 `make_opengl` WoW64 conversion, pointer-or-offset by binding;
  madeira-unix 0116; host test `build/wow64/opengl_ptr_test.c`; gl21 i386 + stress), 3b334b5
  (evidence 2026-10-11-opengl-i386). Gate: i386 immediate 8.42 ms/frame (draw 4.39 ms) vs 16 ms
  budget: host-side serves i386. IPA `ddd76f9a…`; report `reports/zink-3.md`. Follow-up PLA-189
  (i386 persistent maps). Tried: Quake II GOG not installable (PLA-188), NWN Diamond `binkw32.dll`
  missing (PLA-187).
- Zink chunk 2, WGL completeness on x86-64: 22e2abc (madeira-unix 0115 surface at first
  make-current and layer hand-off, PLA-180; wine-unix 0028 no GL without an EGL display, PLA-179;
  mesa 0028 sRGB windows; `tools/compat/probes/wgl`), 28308e7 (decision 0088, evidence
  2026-10-11-opengl-wgl-completeness); IPA `2e27a299…`; report `reports/zink-2.md`. DDNet and
  Teeworlds are not on the account; Neverwinter Nights: EE (GOG, PLA-185) reaches gameplay at
  50 fps instead; Baldur's Gate EE (PLA-183) is blocked by OpenAL (PLA-182). PLA-181: custom border
  colours work with `MESA_KK_EXPERIMENTAL=custom_border`, off by default. Follow-ups PLA-184 (MSAA
  rebind crash), PLA-186 (screenshot "No frame to save" on KosmicKrisp, also DXVK).
- Zink 1a/1b: 69db234 (Mesa 0022–0027, MesaEGL/MesaGallium frameworks, +5.2 MB IPA), 8e5eabd
  (madeira-unix 0114 `winios_OpenGLInit`, opengl32 x86-64 unix side, `tools/compat/probes/gl21`);
  decision 0087; GL 3.3 core/compat on the phone, probe 60 fps; HK/P2 unchanged; IPA `78bd08ad…`;
  report `reports/zink-1.md`. The Mesa GL licensing edit (`build/app-notices.json` part `MesaGL`)
  awaits the owner's review.
- DirectX setup skip: 2dff9fe, 56daa55, decision 0086; report `reports/dxsetup-skip.md`.

## Queue (in order; owner-set 2026-10-11)

Owner order: Zink, then PLA-115, then PLA-161; then (owner, 2026-10-11 evening) **every issue
filed along the way in the compat runs**, before any more library titles; then the library and
DiRT 3. Zink (PLA-162) and PLA-161 are done; PLA-115 is parked after two diagnosis-only attempts
(it closes the sweep); the sweep is running (2.10c).

1. **Issue sweep: the issues filed during this speedrun (PLA-113 onwards), one chunk at a time,
   in this order** (one writer each, the usual gates; a chunk ends fixed, or with solid evidence
   and the next step in its issue; a title a fix unblocks is replayed in the same chunk). Supervisor
   order by impact; a chunk may take two related issues:
   1. **Done** (ad2fce2). [PLA-182](https://linear.app/playportdev/issue/PLA-182) OpenAL32 missing (High; unblocks BGEE [PLA-183](https://linear.app/playportdev/issue/PLA-183), needed by DiRT 3): Proton parity
      first; GOG's declared dependencies / `oalinst` through the 0079 mechanism. Shipping OpenAL
      Soft needs the owner (licensing): record it as an owner decision, don't ship.
   2. **Done** (58534bb, reverted to the broad 0030 in 2.3). [PLA-193](https://linear.app/playportdev/issue/PLA-193) KosmicKrisp on iOS 27: narrow workaround 6's vertex-stage cost (Portal 2 +3.5 ms
      GPU a frame since 246365a) and survey the other OS-27-disabled workarounds (High; filed from
      PLA-161).
   3. **Done** (6748815, wine-unix 0030). [PLA-177](https://linear.app/playportdev/issue/PLA-177) one `Software\Wow6432Node` key hides HKLM\Software from i386 callers (High;
      supervisor lean: server fallback patch with a detection log line).
   4. **Done** (0118, 0119, wine-pe 0037). [PLA-164](https://linear.app/playportdev/issue/PLA-164) GoldSrc `-soft` host fault at a thread's stack end, and an unhandled host fault
      looping 2000 redeliveries instead of ending the title (High; Counter-Strike `-soft`).
   5. **Done** (0c51652). [PLA-120](https://linear.app/playportdev/issue/PLA-120) rpmalloc lock deadlock after a WoW64 child aborts, with [PLA-117](https://linear.app/playportdev/issue/PLA-117) Wine dxdiag
      failing (Mortal Kombat Komplete Edition, PLA-36).
   5b. **Done** (c3cda59). [PLA-199](https://linear.app/playportdev/issue/PLA-199) Local import space check (same-volume clone counted as a copy), then the MKKE replay.
   5c. **Done** (60d6de9, fex 0025). [PLA-202](https://linear.app/playportdev/issue/PLA-202) FEX reduced precision rounds `fild`/`fistp` m64 (MKKE; High, from 2.5b).
   6. **Done** (af4be23, 0122; PLA-122 open). [PLA-118](https://linear.app/playportdev/issue/PLA-118) per-page NtProtect RX flips scan every JIT mapping (Football Manager 2022,
      PLA-114), then [PLA-122](https://linear.app/playportdev/issue/PLA-122) JWE DRM wild store if time allows (PLA-41).
   6b. **Done** (9d40a49, wine-pe 0038). [PLA-203](https://linear.app/playportdev/issue/PLA-203) FEX `NoExec` after RW→RX (High, from 2.6).
   6c. **Done** (e4d8aa6: madeira-unix 0124, wine-pe 0039). [PLA-205](https://linear.app/playportdev/issue/PLA-205) stale anon JIT alias after `MEM_RELEASE`, with [PLA-201](https://linear.app/playportdev/issue/PLA-201) re-measured (moved up from 13b).
   7. **Done** (ee2b96e, 8fa6792; decisions 0091, 0092). [PLA-192](https://linear.app/playportdev/issue/PLA-192) Steam apps sharing an install folder (Counter-Strike and Half-Life), with
      [PLA-123](https://linear.app/playportdev/issue/PLA-123) a compat default lost when the player types their own Runtime keys.
   8. **Done** (d506e4a, 81ab29b; mesa 0031–0033, decision 0093; PLA-189 design only). Zink follow-ups: [PLA-184](https://linear.app/playportdev/issue/PLA-184) MSAA rebind crash, [PLA-189](https://linear.app/playportdev/issue/PLA-189) i386 persistent maps, [PLA-191](https://linear.app/playportdev/issue/PLA-191)
      Half-Life GPU time, [PLA-181](https://linear.app/playportdev/issue/PLA-181) custom border colours (decision 0088 stands unless the fix
      is per-context), [PLA-198](https://linear.app/playportdev/issue/PLA-198) windowed GL GoldSrc host-mutex hang.
   8a. **Done** (a096230; decision 0094: restart only for a short pool). [PLA-209](https://linear.app/playportdev/issue/PLA-209) avoid the restart between first-run prerequisites and the game (owner, 2026-10-12).
   8a2. **Done** (239283e; decision 0095). [PLA-208](https://linear.app/playportdev/issue/PLA-208) launch screen: "Installing dependencies…" and its own step dot (owner, 2026-10-12; after 8a, whose outcome sets the restart wording).
   8a2b. **Done** (825c102; decision 0096). [PLA-212](https://linear.app/playportdev/issue/PLA-212) run the installer of Steam's standard VC++ `.cmd` directly (owner approved 2026-10-12: outcome parity with Steam/Proton, not the exact process).
   8b. **Done** (6fd94a1, 597d663; decision 0097). [PLA-197](https://linear.app/playportdev/issue/PLA-197) display source and mode emulation: a named `ChangeDisplaySettings` fails (GoldSrc
      `-soft`, any mode-changing game; High, filed from PLA-164).
   9. **Done** (86d91c1; mesa 0035). [PLA-186](https://linear.app/playportdev/issue/PLA-186) in-game Screenshot finds no frame on KosmicKrisp: a capture route with no
      present cost only (`framebufferOnly = NO` is the owner's performance call: record, don't do).
   10. (Split 2026-10-12: 10a **Done** (74f1212; decision 0098) = PLA-167, 168, 157, 166; 10b **Done** (68fa619) = PLA-220 (inserted), PLA-173, PLA-200; 10c = PLA-224, PLA-223 (inserted); 10d = PLA-222 (inserted).) Prefix and loader leftovers ([PLA-200](https://linear.app/playportdev/issue/PLA-200) async abort of killed threads at a safe point goes here): [PLA-167](https://linear.app/playportdev/issue/PLA-167) `xaudio2_9redist.dll`, [PLA-168](https://linear.app/playportdev/issue/PLA-168) wineboot machine
      values, [PLA-157](https://linear.app/playportdev/issue/PLA-157) win32u unix init per process, [PLA-173](https://linear.app/playportdev/issue/PLA-173) winedbg for arm64ec,
      [PLA-166](https://linear.app/playportdev/issue/PLA-166) typelibs / CurrentControlSet (low; evidence only if no title needs it).
   11. Titles filed broken in the runs ([PLA-122](https://linear.app/playportdev/issue/PLA-122)/[PLA-114](https://linear.app/playportdev/issue/PLA-114)/[PLA-41](https://linear.app/playportdev/issue/PLA-41) anti-tamper check trace goes here): [PLA-187](https://linear.app/playportdev/issue/PLA-187) NWN Diamond `binkw32.dll`, [PLA-188](https://linear.app/playportdev/issue/PLA-188) Quake II not
       installable from GOG; and [PLA-121](https://linear.app/playportdev/issue/PLA-121) videos invisible on Vulkan (reproduce first).
   12. Tooling: [PLA-156](https://linear.app/playportdev/issue/PLA-156) and [PLA-124](https://linear.app/playportdev/issue/PLA-124) (pad/wait after play, `--settings` lists, AFC remove,
       desktop capture), [PLA-119](https://linear.app/playportdev/issue/PLA-119) detect an iOS system alert and fail fast.
   13. [PLA-169](https://linear.app/playportdev/issue/PLA-169) per-title prefixes: the research note and decision proposal only (no
       implementation until the owner decides).
   13b. **Done in 2.6c** (wine-pe 0039). [PLA-201](https://linear.app/playportdev/issue/PLA-201) FEX on iOS: code rewritten in place not retranslated after `FlushInstructionCache`
      (High; check before PLA-115).
   14. ([PLA-206](https://linear.app/playportdev/issue/PLA-206) WoW64 4 KiB SMC trap gap goes with this item.) [PLA-115](https://linear.app/playportdev/issue/PLA-115) third attempt (Mono with SMC tracking off), from the next steps in the issue.
   14b. [PLA-213](https://linear.app/playportdev/issue/PLA-213) return dead pseudo-process image ranges to the pool head (lowered 2026-10-12: 0096 removed the VC++ pair restart; general fix for other multi-process installers).

   Not in the sweep (owner decisions): [PLA-155](https://linear.app/playportdev/issue/PLA-155), [PLA-176](https://linear.app/playportdev/issue/PLA-176), [PLA-190](https://linear.app/playportdev/issue/PLA-190). Not filed in the runs, so left
   in Not yet scheduled: [PLA-99](https://linear.app/playportdev/issue/PLA-99) (SteamStub x86).
2. **Phase 2, the library A–Z** (resumes here; #1 #DRIVE Rally is done, next is **9 Years of
   Shadows**, GOG 2099051765): the triage in the build area
   (`.work/agent-notes/speedrun/library-triage.md`: ProtonDB, anti-cheat, API, size, verdict per
   title) and the PLA-163 import census (`.work/agent-notes/pla163/census.md`). Skip SKIP
   verdicts, empty depots, dead services, and a second store copy once one is tested. One
   per-game worker per title (brief above); the supervisor creates the Linear issue from the report.
3. **Phase 1 rest: DiRT 3 Complete Edition** (Local import, 15 GB, i386 D3D9,
   [PLA-24](https://linear.app/playportdev/issue/PLA-24)); needs ~17 GB free (House of Golf 2 is already uninstalled, 2.5); its
   own OpenAL installscript now runs (PLA-158).

Not yet scheduled (pick up after the queue or when a title needs them):
an XNA title to first frame + 30 s (Mono's SMC cost under i386 FEX),
[PLA-99](https://linear.app/playportdev/issue/PLA-99) (SteamStub 3.1 x86; full Castle Crashers).
Everything else filed in the runs is in the issue sweep (Queue item 2).

Owner decisions open: [PLA-190](https://linear.app/playportdev/issue/PLA-190) (Mesa GL notices review, before a release), [PLA-155](https://linear.app/playportdev/issue/PLA-155) (Wine Mono size),
[PLA-169](https://linear.app/playportdev/issue/PLA-169) (per-title prefixes),
[PLA-176](https://linear.app/playportdev/issue/PLA-176) (`__PAGEZERO` experiment). The owner signs
off the release app's launcher/dialog behaviour (decision 0068) before a merge.

## Phone state (2026-10-12 13:00)

Unattended mode on. Installed: Hollow Knight, Portal 2, Counter-Strike (0.9 GB), and the probe
imports `dir-gl21`, `dir-wgl`, `dir-gl21-i386`, `dir-childabort`, `dir-x87m64-i386` (small; House of Golf 2 was
uninstalled 2026-10-12 for MKKE; `Documents/Import/` holds their
folders). Everything else tested was uninstalled through the UI (latest: NWN:EE, NWN Diamond,
BGEE, Counter-Strike, Half-Life); the prefix keeps NWN:EE's edited `settings.tml` under the user's
Documents. About 13.0 GB free (Duck Paradox, Cat Mail Co. Demo and BGEE installed and removed since); check `pp phone status` before any install. The prefix records
Castle Crashers' DXSETUP as skipped (decision 0086), Counter-Strike's VC++ 2019 x86 as 1638
(downgrade refused: HoG2 installed 14.38) and x64 as success, and the VC++ 2022 HasRunKeys.
Owner files in Documents (e.g. `Playport-PLA-147.ipa`) stay.

## Where things are

- Build-area queue (kept in step with this plan, which wins): `.work/agent-notes/speedrun/queue.md`.
- Per-game and per-chunk reports: `.work/agent-notes/speedrun/reports/` (copies of each child's
  report; `p2-NN-<slug>.md` for Phase 2 titles).
- Brief: `.work/agent-notes/speedrun/brief.md`. Library triage:
  `.work/agent-notes/speedrun/library-triage.md`. Census and prefix diff: `.work/agent-notes/pla163/`.

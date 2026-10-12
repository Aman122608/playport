# Plan: compatibility speedrun

**Date:** 2026-10-08 (started); paused 2026-10-09; resumed 2026-10-10/11. **Kind:** execution
plan, in progress. **Linear:** [PLA-112](https://linear.app/playportdev/issue/PLA-112) (umbrella;
each game has its own Compatibility issue, each cross-title fault its own issue).

**Branch:** `dev/pla-152-gdi-windows` (from `main` 6ad89e8), unmerged and unpushed; it carries
all of the speedrun's work since 2026-10-10. (`dev/compat-speedrun`, the first leg, is merged
into `main`.) Merge to `main` with `--no-ff` only when the owner asks. Last updated
2026-10-12 21:20 at 007364a (+ this plan commit).

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

Latest IPA on the phone: `50ab857755f116679e8da274f4a42abe8bf5de5498142874bd6834312bde974b`
(8146ede: fex 0034, strict split locks by default on iOS). Hollow Knight and Portal 2 reach their first frame on it.

## Running now

- **Issue sweep 2.11j, [PLA-99](https://linear.app/playportdev/issue/PLA-99) SteamStub 3.1 x86 and x64 with encrypted code not removed**
  (owner 2026-10-12): compare Madeira's route around SteamStub, implement the removal, replay
  Castle Crashers (and Bloodstained: Curse of the Moon / Tetris Effect if in the library).
  Writer started 2026-10-12 from the plan commit after 007364a. Run id in
  `.work/agent-notes/speedrun/queue.md`; report `.work/agent-notes/speedrun/reports/sweep-11j-steamstub.md`.

Done before (newest first):
- Sweep 2.11i, [PLA-243](https://linear.app/playportdev/issue/PLA-243) contended split `lock xadd` loses updates, done, fixed (8146ede, 007364a).
  FEX's handler for an access across 16 bytes is two CAS loops and returns on a tear.
  `StrictInProcessSplitLocks` serialises every split access it completes; fex 0034 makes it the
  default on iOS (`FEX_STRICTINPROCESSSPLITLOCKS=0` still turns it off). The `splitlock` probe
  loses 0 of 4000 on x86-64 and i386 (before: 8 and 5); single-threaded cost not measurable,
  contended `xadd` 4–8 % slower ([evidence](../evidence/2026-10-12-split-lock-tear.md)). JWE
  unchanged: first frame +38.7 s, WatchDog at 75 s. IPA `50ab8577…` installed; HK and Portal 2
  reach their first frames on it. Report `.work/agent-notes/speedrun/reports/sweep-11i-split-tear.md`.
  Linear: PLA-243 In Review; PLA-41 and [PLA-244](https://linear.app/playportdev/issue/PLA-244) commented (the Mach `cmpxchg` and FEX split-handler lock gap folded into PLA-244).
- Sweep 2.11h, [PLA-241](https://linear.app/playportdev/issue/PLA-241) JWE `c000001d` at RIP `0x142d74711`, done, fixed (c13a442, 80cfc1a).
  A self-modifying write FEX missed: the protected code decrypts its next instruction
  (`xor dword [rip+0]` → `cpuid`) in a page FEX had trapped, but the page's 16 KiB host page held
  still-writable RWX guest pages, so the write did not fault and the encrypted bytes ran
  (`rax=0xf20581b0` every run). fex 0033 traps and invalidates whole host pages on iOS. JWE now
  reaches its first frame (+38 s, black); later the WatchDog ends it at 74 s while `fTask W0` runs
  about 1.3 million split locks (PLA-242's cost), and one logged run ended `c0000005` after a
  thread-context check. `StrictInProcessSplitLocks` A/B: no change
  ([evidence](../evidence/2026-10-12-jwe-ecode-smc.md)). IPA `adf03079…` installed; HK and Portal 2
  reach their first frames on it. Report `.work/agent-notes/speedrun/reports/sweep-11h-jwe-ecode.md`.
  Linear: PLA-241 In Review; PLA-41 and PLA-243 commented; [PLA-244](https://linear.app/playportdev/issue/PLA-244) raised to High (JWE's WatchDog at 74 s, split-lock bound); filed [PLA-245](https://linear.app/playportdev/issue/PLA-245) (High, guest context reads).
- Sweep 2.11g, [PLA-242](https://linear.app/playportdev/issue/PLA-242) guest split locks, done (dad07e7, 83cac4d, c43e42d).
  The `splitlock` probe (x86-64 and i386) measured 61 to 75 µs a `lock xadd`/`xchg` across 16 or
  64 bytes on x86-64 (`lock cmpxchg`: about 14 µs, emulated in the Mach handler); every result was
  correct, and contended `lock xadd` loses a few updates in FEX's two-CAS handler (before and after).
  wine-pe 0040 and fex 0032 log the first 8 and every 65536th split lock with a count, and
  madeira-unix 0133 skips a read's region query: 35 to 48 µs, no handler changed. Handling it in the
  Mach handler was not done (not atomic against aligned access; its redelivery terminator).
  JWE ends `c000001d` at the same RIP `0x142d74711` after 32 s instead of 44 s, so split-lock time
  does not decide it ([evidence](../evidence/2026-10-12-guest-split-locks.md)). IPA `fc335c10…`
  installed; HK and Portal 2 reach their first frames on it. Report
  `.work/agent-notes/speedrun/reports/sweep-11g-split-locks.md`.
  Linear: PLA-242 In Review; PLA-241 and PLA-41 commented; follow-ups [PLA-243](https://linear.app/playportdev/issue/PLA-243) (contended split `xadd` loses updates, inserted 11i) and [PLA-244](https://linear.app/playportdev/issue/PLA-244) (Low, faster atomic path, NtProtect log volume).
- Sweep 2.11f, [PLA-238](https://linear.app/playportdev/issue/PLA-238) JWE (Epic), done, the stall fixed; a new failure (fe88cfd, 4b4a66b).
  Not a lost I/O completion: wine-unix 0035 (`env.WINEDEBUG=warn+iolog`, session only) shows every
  overlapped read queued its packet. The runtime's fault-loop breaker killed Wwise's bank thread at
  about +9.6 s, taking 50 handled split-lock faults in a loop for a livelock; madeira-unix 0132 stops
  counting alignment faults. JWE now runs its protected code at full load and ends `c000001d` after
  44 s at guest RIP `0x142d74711` (FEX: invalid instruction), no frame
  ([evidence](../evidence/2026-10-12-jwe-io-completion.md)). Fastsync ruled out. IPA `98fb3223…`
  installed; HK and Portal 2 reach their first frames on it. Report
  `.work/agent-notes/speedrun/reports/sweep-11f-jwe-io.md`.
  Linear: PLA-238 In Review; PLA-41 commented; follow-ups [PLA-241](https://linear.app/playportdev/issue/PLA-241) (JWE `c000001d` in `.ecode`, inserted 11h) and [PLA-242](https://linear.app/playportdev/issue/PLA-242) (split-lock cost, inserted 11g).
- Sweep 2.11e, [PLA-236](https://linear.app/playportdev/issue/PLA-236) NWN Diamond black at its Bink intro, done
  (4d93ca2, dc1f6d8). The Bink movies are GDI frames painted into the game window; the guest windows ended at
  a drawable Zink took, not a frame shown, so that window was never copied. The guest windows now
  give way at the first presented frame (`PacedMetalLayer.onFirstPresent`). On IPA `895abae8…`, the
  movies show, taps skip them, the OpenGL menu appears and answers taps (Load Game). HK and Portal 2
  gates ok. games.json: Playable with issues (nothing past the menu played). Evidence
  2026-10-12-nwn-bink-intro; report `.work/agent-notes/speedrun/reports/sweep-11e-nwn-bink.md`.
  Linear: PLA-236 In Review; PLA-187 Playable with issues (menu reached; play into a module still to check).
- Sweep 2.11d2, [PLA-239](https://linear.app/playportdev/issue/PLA-239) DXVK: an in-process shared handle for an unshared texture on
  request, done (d24622c, c2c5e77; decision 0102, `patches/dxvk` 0005). The first `GetSharedHandle` on a
  texture made unshared moves it to dedicated exportable memory (a copy, submitted at once) and
  returns the D3DKMT global handle 0101's Metal-heap table resolves. **Valheim's intro plays on
  Vulkan**, and so does Hollow Knight's New Game prologue (slot 3 by pad); HK and Portal 2 reach
  their first frames (IPA e99cb8e7, [evidence](../evidence/2026-10-12-vulkan-unity-video-unshared-handle.md)).
  Report `.work/agent-notes/speedrun/reports/sweep-11d2-unshared-handle.md`.
  Linear: PLA-239, PLA-65 and PLA-121 In Review (videos on Vulkan); PLA-232 goal met; PLA-51 commented (page stays DXMT until a full Vulkan check).
- Sweep 2.11d, [PLA-232](https://linear.app/playportdev/issue/PLA-232) D3D11 shared textures on Vulkan, done, Valheim not fixed (48c077c, 7e57276; decision
  0101). A handle map alone cannot alias memory across devices, so shared memory goes through
  KosmicKrisp's Metal heaps within the process (wine-unix 0034, madeira-unix 0131), and DXVK
  opens a D3DKMT adapter without a LUID (dxvk 0003; dxvk 0004 logs). Valheim's Media Foundation
  textures are now shared without error, but its intro stays black: Unity asks for the handle of
  an **unshared** render texture ([evidence](../evidence/2026-10-12-vulkan-shared-texture-metal.md)),
  which needs its own decision. Hollow Knight and Portal 2 reach their first frames on Vulkan (IPA
  6f430560). Report `.work/agent-notes/speedrun/reports/sweep-11d-shared-textures.md`.
  Linear: PLA-232 In Review; PLA-121, [PLA-65](https://linear.app/playportdev/issue/PLA-65) (same cause) and PLA-51 commented; follow-ups [PLA-239](https://linear.app/playportdev/issue/PLA-239) (unshared texture handle on request, inserted 11d2) and [PLA-240](https://linear.app/playportdev/issue/PLA-240) (Low, no display GPU/LUID).
- Sweep 2.11c, [PLA-41](https://linear.app/playportdev/issue/PLA-41) JWE, done, not fixed (2aa1b34; commit
  "wine-unix 0033: a wait log; JWE waits in its job loop (PLA-41)"). `patches/wine-unix` 0033 logs
  what a long wait waits on (`env.WINEDEBUG=warn+waitlog`, session only). Main and Render are idle
  in the engine's thread-affinity job loop (their own auto events plus a shared one), every worker
  and the `fIO` completion pool are idle, and EOS's HTTPS to `api.epicgames.dev` completed by +7.5 s:
  no job is ever produced. Next: the I/O completion path, then the role events' setters
  ([evidence](../evidence/2026-10-12-jwe-watchdog.md)). IPA `ddb09c3e…` installed; HK and Portal 2
  reach their first frames on it.
  Linear: PLA-41 commented; follow-up [PLA-238](https://linear.app/playportdev/issue/PLA-238) (I/O completion trace, inserted 11f). IPA `ddb09c3e…` installed; HK/P2 gates ok.
- Sweep 2.11b, i386 Inno setup ([PLA-235](https://linear.app/playportdev/issue/PLA-235) High, then [PLA-233](https://linear.app/playportdev/issue/PLA-233)) (40537cb, 2bd6954, b524e28):
  - **PLA-235 fixed** (`patches/fex` 0031): reduced x87 now writes the whole 16-byte register slot,
    a `fild m64`'s integer in its upper half, and a slow-path `fistp m64` uses it when it converts to
    the double on the stack, so Delphi's `Move` across blocks is exact. The `x87m64` probe's new
    branch and `Move` cases fail on `12062a00` and pass on `13f8dda9` (evidence
    2026-10-12-x87-m64-across-blocks).
  - **PLA-233 fixed, not FEX:** GOG's Inno build carries code page 1250 in its AnsiString literals
    and the app shipped no `c_1250.nls`, so every script function got an empty name (reproduced
    under desktop Wine). The bundle's `nls/` now has every Windows ANSI code page (evidence
    2026-10-12-gog-inno-ansi-code-page).
  - **NWN Diamond (PLA-187):** the support setup completes at the default x87 setting, places
    `binkw32.dll` and the `.ini` files; the game starts on OpenGL through Zink but shows nothing
    through first frame +75 s (main thread in `binkw32.dll`). Still Broken; next step a Play with
    Escape during the intro, then the Bink path.
  - IPA `13f8dda9…` (dev; installed): HK first frame +9.9 s (menu), Portal 2 +5.8 s. No `pp perf`.
    Report `.work/agent-notes/speedrun/reports/sweep-11b-inno.md`.
  Linear: PLA-235 and PLA-233 In Review; PLA-187 commented, its black intro split to [PLA-236](https://linear.app/playportdev/issue/PLA-236) (inserted 11e); [PLA-237](https://linear.app/playportdev/issue/PLA-237) (Low, `c_10000.nls`). Owner: Vulkan is the direction, DXMT to be dropped (decision 0100); PLA-232 approved on Vulkan only (inserted 11d).
- Sweep 2.11, titles filed broken (16ffed2, 1b1b27e, dda3998, 717d2b0, 8c0847c, a4c3f5e):
  - **PLA-187 (NWN Diamond):** `binkw32.dll` is a GOG `support` file (`app/`), placed by the build's
    `temp_executable`, GOG's Inno setup, which GOG Galaxy and Heroic run. Decision 0099: Play runs it
    once per build; a region-less depot language (`en`) matches `en-US` (the install had been French).
    On the phone the i386 setup does not finish: reduced x87 corrupts its loader's path (Delphi
    `fild`/`fistp` across blocks; `Path not fpund.` box), and at 80-bit x87 its script cannot import
    `ExpandConstant`. Still Broken; evidence 2026-10-12-gog-support-setup.
  - **PLA-188 (Quake II):** GOG has only a generation-1 build; design recorded, not built
    (evidence 2026-10-12-gog-generation-1).
  - **PLA-121:** reproduced on Valheim's intro on Vulkan: DXVK cannot share a texture
    (no `VK_KHR_external_memory_win32`); next step `VK_EXT_external_memory_metal` (evidence
    2026-10-12-vulkan-video-shared-texture).
  - **PLA-122/114/41:** replayed first; neither fails in its anti-tamper now. FM2022: the server
    refuses re-validation for 24 h (`88500006`). JWE: the engine starts, its WatchDog ends it at
    about 47 s with `Main` waiting. No logging patch (evidence 2026-10-12-anti-tamper-replay).
  - IPA `12062a00…` (16ffed2, GOG client only; no HK/P2 gate). Report
    `.work/agent-notes/speedrun/reports/sweep-11-broken-titles.md`.
  Linear: PLA-122 In Review; PLA-187/188/121/114/41 commented. Follow-ups [PLA-235](https://linear.app/playportdev/issue/PLA-235) (High, FEX i386 `fild`/`fistp m64` across blocks; inserted 11b), [PLA-233](https://linear.app/playportdev/issue/PLA-233) (Inno PascalScript import; 11b), [PLA-234](https://linear.app/playportdev/issue/PLA-234) (GOG generation-1 install), [PLA-232](https://linear.app/playportdev/issue/PLA-232) (DXVK shared textures over Metal; needs a decision, owner). FM2022 replay after 2026-10-13 16:00; JWE wait (PLA-41) inserted 11c.
- **Done** (d331108, 9223798, and the docs commit after them). **Issue sweep 2.10f, x86-64 RWX page reuse ([PLA-228](https://linear.app/playportdev/issue/PLA-228), High):** in the x86-64 childabort `terminate`
  phase, about once in 200 allocations an RWX page freed by one thread and reused at once by another
  loses FEX's executable range (`[iOS-xquery] MISS`, NoExec, `c0000005`); likely the ARM64EC memory
  notifications without a mutex. Writer started 2026-10-12 from the plan commit after 4dcbc75. Run id
  in `.work/agent-notes/speedrun/queue.md`; report `.work/agent-notes/speedrun/reports/sweep-10f-rwx-reuse.md`.
  Testing budget: probe runs (`--wait 40`), then one HK + P2 first-frame+10 on the final IPA; no P2 perf.
  Outcome: **fixed** (fex 0030). The free notification after the free, without the mutex the iOS port
  dropped, removed the range another thread had just been given. It now removes only what is not
  committed again, under `IntervalsLock`. New childabort `rwx` phase: on `af75dafe` it fails within
  milliseconds; on `4b3b3b50` it shows 0 faults with 3,866 pages reused at once, and `terminate` ends
  `done ok`. Final IPA `4b3b3b50…`: HK first frame +9.91 s (main menu), P2 +5.99 s. Evidence
  2026-10-12-rwx-reuse. Follow-up: an execute fault at a page start reads below the page in native code.
  Linear: PLA-228 In Review.
  Linear: PLA-228 In Review. Follow-up [PLA-231](https://linear.app/playportdev/issue/PLA-231) (Low; ARM64EC execute-fault dispatch reads below the page; NotifyMemoryProtect window).
- **Done** (ac0c7bd). **Issue sweep 2.10e, `pp ui --wait` bound ([PLA-227](https://linear.app/playportdev/issue/PLA-227), inserted by the owner's question 2026-10-12):**
  `--wait` keeps the 180 s JIT allowance after JIT is ready, so `--wait 180` waits about 365 s and hung
  probes sat idle about 340 s each. Count `--wait` from JIT ready; optional idle bound. Host tests;
  one short phone run that shows it. Writer started 2026-10-12 from the plan commit after d63611a.
  Run id in `.work/agent-notes/speedrun/queue.md`; report
  `.work/agent-notes/speedrun/reports/sweep-10e-ui-wait.md`.
  Outcome: `--wait` counts from the first `runtime started` mark (a launch gets 180 s for JIT first; a run
  that plays nothing has no JIT allowance); the stop event carries `bound` (wait, jit, idle); `--idle N`
  (default off) is new. Phone: HK with `--wait 20` stopped at 32.8 s (was about 200 s); the childabort
  probe no longer hangs on `af75dafe`. PLA-227 In Review.
  Linear: PLA-227 In Review.
- **Done** (b0b09cc…2be70f9, and the docs commit after it). **Issue sweep 2.10d, killed threads (the rest of [PLA-200](https://linear.app/playportdev/issue/PLA-200)):**
  [PLA-222](https://linear.app/playportdev/issue/PLA-222), [PLA-226](https://linear.app/playportdev/issue/PLA-226), [PLA-225](https://linear.app/playportdev/issue/PLA-225):
  - **PLA-222 fixed for `TerminateThread`** (madeira-unix 0130): the abort of a thread killed
    alone rings FEX's suspend doorbell, and `trap_handler` ends the thread at the next block's
    `brk #0xCAFE`. The i386 childabort `terminate` phase now passes: 0 of 120 terminated workers
    ran on (114 before). Its `crash` phase passes too.
  - Not covered: killed threads in in-process waits (waking them corrupted the shared futex
    queue) and the threads of a terminating process (ringing their doorbell broke the next
    child's start). Both keep 0127's behaviour.
  - **PLA-226 fixed** (fex 0029): reproduced with a new x86-64 childabort. ARM64EC `ThreadTerm`
    freed a running victim's state, and the phase hung in round 1. The state is now kept. The
    x86-64 phase then stops on a separate RWX page reuse race (`[iOS-xquery] MISS`), a follow-up.
  - **PLA-225 not changed** (not contained); a design is in the evidence.
  - Final IPA `af75dafe…`: HK first frame +9.82 s, P2 +5.91 s. Evidence
    2026-10-12-killed-threads; report `.work/agent-notes/speedrun/reports/sweep-10d-killed-threads.md`.
  - Follow-ups for the supervisor: a killed thread in an in-process wait; a terminating process's
    threads busy in FEX; the x86-64 RWX reuse race.
  Linear: PLA-222/226 In Review; PLA-225 open (design); PLA-200 open. Follow-ups [PLA-228](https://linear.app/playportdev/issue/PLA-228) (High, RWX page reuse loses FEX's executable range; inserted 10f), [PLA-229](https://linear.app/playportdev/issue/PLA-229), [PLA-230](https://linear.app/playportdev/issue/PLA-230) (inserted 14c).
- **Done** (473ee87, and the docs commit after it). **Issue sweep 2.10c, FEX thread faults:** [PLA-224](https://linear.app/playportdev/issue/PLA-224) an x86-64 thread
  faulted at its first syscall stub, [PLA-223](https://linear.app/playportdev/issue/PLA-223) a busy TerminateThread victim faulted in libwow64fex
  and kept the FEX lock:
  - **PLA-224 fixed** (madeira-unix 0128): not a reused TEB. The stale-pointer heal pointed the
    child's FEX `KiUserExceptionDispatcher` at the session's ntdll copy, so x64 syscalls left the
    child's copy. It now resolves an ownerless copy for the process that copied it. The x86-64
    cpuid probe reports `done ok` on 6 threads, and i386 too.
  - **PLA-223 fixed in part** (fex 0027, 0028; madeira-unix 0129): the suspend in
    `BTCpuThreadTerm` may not stop the thread on iOS, so its state is kept. A FEX write hold is
    now stamped in the TEB, and the real suspend no longer holds a thread inside one. The
    `terminate` phase runs all 30 rounds with no fault or hang. 114 of 120 busy victims still ran
    on past the probe's 500 ms check (PLA-200's case: a killed thread busy in FEX).
  - No shared cause.
  - IPA `5584d3fe…`: HK first frame +9.87 s, P2 +5.81 s. Evidence 2026-10-12-fex-thread-faults;
    report `.work/agent-notes/speedrun/reports/sweep-10c-fex-threads.md`.
  - Follow-ups for the supervisor: end a killed thread busy in FEX (doorbell), free a terminated
    thread's FEX state after it ends, and the same race in ARM64EC `ThreadTerm`.
  Linear: PLA-224/223 In Review. Follow-ups [PLA-225](https://linear.app/playportdev/issue/PLA-225) (fex 0027 leak, Low) and [PLA-226](https://linear.app/playportdev/issue/PLA-226) (same race on ARM64EC), inserted at 10d with [PLA-222](https://linear.app/playportdev/issue/PLA-222).
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
(it closes the sweep); the sweep is running (2.11j).

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
   10. (Split 2026-10-12: 10a **Done** (74f1212; decision 0098) = PLA-167, 168, 157, 166; 10b **Done** (68fa619) = PLA-220 (inserted), PLA-173, PLA-200; 10c **Done** (473ee87) = PLA-224, PLA-223 (inserted); 10d **Done** (2be70f9) = PLA-222, PLA-226, PLA-225 (inserted); 10e **Done** (ac0c7bd) = [PLA-227](https://linear.app/playportdev/issue/PLA-227) `pp ui --wait` bound (inserted, owner); 10f **Done** (d331108; fex 0030) = [PLA-228](https://linear.app/playportdev/issue/PLA-228) x86-64 RWX page reuse loses FEX's executable range (inserted, High).) Prefix and loader leftovers ([PLA-200](https://linear.app/playportdev/issue/PLA-200) async abort of killed threads at a safe point goes here): [PLA-167](https://linear.app/playportdev/issue/PLA-167) `xaudio2_9redist.dll`, [PLA-168](https://linear.app/playportdev/issue/PLA-168) wineboot machine
      values, [PLA-157](https://linear.app/playportdev/issue/PLA-157) win32u unix init per process, [PLA-173](https://linear.app/playportdev/issue/PLA-173) winedbg for arm64ec,
      [PLA-166](https://linear.app/playportdev/issue/PLA-166) typelibs / CurrentControlSet (low; evidence only if no title needs it).
   11. **Done** (16ffed2–8c0847c; decision 0099). Titles filed broken in the runs ([PLA-122](https://linear.app/playportdev/issue/PLA-122)/[PLA-114](https://linear.app/playportdev/issue/PLA-114)/[PLA-41](https://linear.app/playportdev/issue/PLA-41) anti-tamper check trace goes here): [PLA-187](https://linear.app/playportdev/issue/PLA-187) NWN Diamond `binkw32.dll`, [PLA-188](https://linear.app/playportdev/issue/PLA-188) Quake II not
       installable from GOG; and [PLA-121](https://linear.app/playportdev/issue/PLA-121) videos invisible on Vulkan (reproduce first).
   11b. **Done** (40537cb, 2bd6954; fex 0031, every ANSI code page; NWN Diamond past its setup, black at its intro). [PLA-235](https://linear.app/playportdev/issue/PLA-235) FEX i386 `fild`/`fistp m64` (High) and [PLA-233](https://linear.app/playportdev/issue/PLA-233) Inno PascalScript import (inserted 2026-10-12 from 2.11); replay NWN Diamond.
   11c. **Done, not fixed** (wine-unix 0033 wait log; JWE idle in its job loop, network fine; next the I/O completion path). [PLA-41](https://linear.app/playportdev/issue/PLA-41) JWE: what Main waits on before its WatchDog (inserted 2026-10-12 from 2.11); FM2022 ([PLA-114](https://linear.app/playportdev/issue/PLA-114)) replay once after 2026-10-13 16:00.
   11d. **Done, Valheim not fixed** (48c077c; decision 0101: Metal heaps within the process; Unity asks for an unshared texture's handle, a follow-up). [PLA-232](https://linear.app/playportdev/issue/PLA-232) DXVK D3D11 shared textures on Vulkan only (owner-approved 2026-10-12; decision 0100): an in-process handle map first, else `VK_EXT_external_memory_metal`; a decision record; Valheim's Vulkan intro plays.
   11d2. **Done** (d24622c; decision 0102, dxvk 0005; Valheim's intro and HK's prologue play on Vulkan). [PLA-239](https://linear.app/playportdev/issue/PLA-239) an in-process shared handle for an unshared D3D11 texture on request (inserted 2026-10-12 from 2.11d); Valheim's Vulkan intro plays; HK's New Game video checked.
   11e. **Done** (4d93ca2; Bink frames are GDI, the guest windows now end at the first frame shown; NWN Diamond's intro and menu show). [PLA-236](https://linear.app/playportdev/issue/PLA-236) NWN Diamond black at its Bink intro (inserted 2026-10-12 from 2.11b).
   11f. **Done, stall fixed** (madeira-unix 0132, wine-unix 0035; JWE now ends `c000001d` at `0x142d74711`, a follow-up). [PLA-238](https://linear.app/playportdev/issue/PLA-238) JWE: trace the I/O completion path with the wait log, then the role events' setters (inserted 2026-10-12 from 2.11c).
   11g. **Done** (dad07e7, 83cac4d; per-fault logging cut, 61–75 µs to 35–48 µs; JWE ends at the same RIP 12 s sooner). [PLA-242](https://linear.app/playportdev/issue/PLA-242) guest split-lock cost: measure, cut per-fault logging, handle earlier; replay JWE (inserted 2026-10-12 from 2.11f).
   11h. **Done, fixed** (c13a442, fex 0033: FEX now traps code by 16 KiB host page; JWE reaches its first frame, then its WatchDog at 74 s). [PLA-241](https://linear.app/playportdev/issue/PLA-241) JWE `c000001d` at `0x142d74711` in protected `.ecode`: memory vs file bytes, who last wrote or protected the page (inserted 2026-10-12 from 2.11f).
   11i. **Done** (8146ede: fex 0034, strict split locks by default on iOS; the probe loses 0). [PLA-243](https://linear.app/playportdev/issue/PLA-243) FEX: a contended split `lock xadd` across 16 bytes loses updates (tear without retry); fix and show with the `splitlock` probe (inserted 2026-10-12 from 2.11g).
   11j. **Running.** [PLA-99](https://linear.app/playportdev/issue/PLA-99) SteamStub 3.1 x86 and x64 with encrypted code (High; owner 2026-10-12): compare Madeira's route, remove the stub, replay Castle Crashers.
   11k. [PLA-194](https://linear.app/playportdev/issue/PLA-194) GOG install script (ISI), version-1 support commands, `.msi` redistributables (owner approved 2026-10-12).
   11l. [PLA-245](https://linear.app/playportdev/issue/PLA-245) `NtGetContextThread` returns rip=0 before a thread starts and a host PC in native code (High; JWE's context check, Mono GC suspension): probe on desktop Wine and the phone, fix, replay JWE.
   11m. [PLA-244](https://linear.app/playportdev/issue/PLA-244) a faster split-lock path that stays atomic (raised to High 2026-10-12: JWE's protected job loses the race with its WatchDog at 74 s, about 1.3 M split locks at 40 µs); replay JWE.
   12. Tooling: [PLA-156](https://linear.app/playportdev/issue/PLA-156) and [PLA-124](https://linear.app/playportdev/issue/PLA-124) (pad/wait after play, `--settings` lists, AFC remove,
       desktop capture), [PLA-119](https://linear.app/playportdev/issue/PLA-119) detect an iOS system alert and fail fast.
   13. **Dropped** (owner 2026-10-12: keep the one shared prefix; PLA-169 Canceled).
   13b. **Done in 2.6c** (wine-pe 0039). [PLA-201](https://linear.app/playportdev/issue/PLA-201) FEX on iOS: code rewritten in place not retranslated after `FlushInstructionCache`
      (High; check before PLA-115).
   14. ([PLA-206](https://linear.app/playportdev/issue/PLA-206) WoW64 4 KiB SMC trap gap goes with this item; fex 0033 (2.11h) traps by 16 KiB host page on WoW64 too, so check first whether it is already fixed.) [PLA-115](https://linear.app/playportdev/issue/PLA-115) third attempt (Mono with SMC tracking off), from the next steps in the issue.
   14b. [PLA-213](https://linear.app/playportdev/issue/PLA-213) return dead pseudo-process image ranges to the pool head (lowered 2026-10-12: 0096 removed the VC++ pair restart; general fix for other multi-process installers).
   14c. [PLA-229](https://linear.app/playportdev/issue/PLA-229) killed thread in an in-process wait and [PLA-230](https://linear.app/playportdev/issue/PLA-230) threads of a terminating process (inserted 2026-10-12 from 2.10d).
   15. **High-priority backlog** (owner 2026-10-12: every High issue still needed goes before the library):
   15a. Triage, mostly host and code: for each of [PLA-40](https://linear.app/playportdev/issue/PLA-40), [PLA-12](https://linear.app/playportdev/issue/PLA-12), [PLA-8](https://linear.app/playportdev/issue/PLA-8), [PLA-55](https://linear.app/playportdev/issue/PLA-55), [PLA-69](https://linear.app/playportdev/issue/PLA-69), [PLA-93](https://linear.app/playportdev/issue/PLA-93), [PLA-132](https://linear.app/playportdev/issue/PLA-132), [PLA-101](https://linear.app/playportdev/issue/PLA-101), [PLA-102](https://linear.app/playportdev/issue/PLA-102), decide whether it still applies on the current branch (later fixes, evidence, one batched phone session at most). Close stale ones with evidence; leave the rest in order below.
   15b. [PLA-132](https://linear.app/playportdev/issue/PLA-132) A12 iPads: FEX emits ARMv8.4 FlagM (`rmif`); detect host features through sysctl, test by forcing them off on our phone.
   15c. [PLA-93](https://linear.app/playportdev/issue/PLA-93) Mono GC suspend holds a thread that owns FEX's CodeInvalidationMutex (after 11l).
   15d. [PLA-101](https://linear.app/playportdev/issue/PLA-101) and [PLA-102](https://linear.app/playportdev/issue/PLA-102) `pp perf` dry/offline modes without side effects; `pp ui` tells a clean exit from the checkpoint it was asked for.
   15e. [PLA-69](https://linear.app/playportdev/issue/PLA-69) Epic: a failed sidecar or receipt refresh blocks the update check.
   15f. [PLA-55](https://linear.app/playportdev/issue/PLA-55) GOG Galaxy SDK connects but never sends.
   15g. [PLA-40](https://linear.app/playportdev/issue/PLA-40) guest TLS for EOS/Unity HTTPS, if 15a finds it still failing.
   15h. [PLA-8](https://linear.app/playportdev/issue/PLA-8) "game debugging could not be prepared" after pairing: diagnose from the reports and code (needs a SideStore/iLoader setup we do not have); record next steps.
   15i. [PLA-12](https://linear.app/playportdev/issue/PLA-12) umbrella: map each reported title to its issue; close it if all are covered.
   Not queued: [PLA-190](https://linear.app/playportdev/issue/PLA-190) (the owner's licensing review).

   Not in the sweep (owner decisions): [PLA-155](https://linear.app/playportdev/issue/PLA-155), [PLA-176](https://linear.app/playportdev/issue/PLA-176), [PLA-190](https://linear.app/playportdev/issue/PLA-190). PLA-99 is now 11j.
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

## Phone state (2026-10-12 21:20)

Unattended mode on. Installed: Hollow Knight, Portal 2, Counter-Strike, Football Manager 2022,
Jurassic World Evolution, Death's Door (Epic), Neverwinter Nights Diamond (GOG, English, 5.1 GB, kept
for the i386 Inno retest), and small probe imports (`dir-wgl`, `dir-cpuid*`, `dir-childabort*`, …;
`Documents/Import/` holds their folders). Photos add-only permission granted. About 15.9 GB free
(2026-10-12 after sweep 2.11e; NWN Diamond kept, it reaches its menu); check `pp phone status` before any install. The prefix records
Castle Crashers' DXSETUP as skipped (decision 0086), Counter-Strike's VC++ 2019 x86 as 1638
(downgrade refused: HoG2 installed 14.38) and x64 as success, and the VC++ 2022 HasRunKeys.
Owner files in Documents (e.g. `Playport-PLA-147.ipa`) stay.

## Where things are

- Build-area queue (kept in step with this plan, which wins): `.work/agent-notes/speedrun/queue.md`.
- Per-game and per-chunk reports: `.work/agent-notes/speedrun/reports/` (copies of each child's
  report; `p2-NN-<slug>.md` for Phase 2 titles).
- Brief: `.work/agent-notes/speedrun/brief.md`. Library triage:
  `.work/agent-notes/speedrun/library-triage.md`. Census and prefix diff: `.work/agent-notes/pla163/`.

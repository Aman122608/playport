# Plans and Linear

Linear (team PLA) is Playport's tracker for everything. Plans hold designs and
execution gates, not a separate backlog. Search before creating an issue; link
its ID/URL in the plan and the plan's repository path in the issue. Update Linear
with progress, blockers, evidence and follow-ups as work proceeds (AGENTS.md).
A game's issue alone does not cover a broader runtime/tooling implementation.
The rule includes draft plans under `.work/plans/`: track them when written,
then promote a scrubbed version to `docs/plans/` before execution. Private research
and account inventories stay in `.work`, not in the plan or Linear.

## Active plans after review

| Plan | Tracking issue | Review outcome |
| --- | --- | --- |
| [Performance review](2026-09-30-performance-review.md) | [PLA-79](https://linear.app/playportdev/issue/PLA-79) | Partial; reconcile historical hypotheses against implemented defaults and current code. |
| [KosmicKrisp FL12_2](2026-10-05-kosmickrisp-fl12_2.md) | [PLA-81](https://linear.app/playportdev/issue/PLA-81) | Research-only draft promoted from `.work/plans/`; refresh old capability/pin assumptions before execution. |
| [Proton ARM64 alignment](2026-10-05-proton-arm64-alignment.md) | [PLA-76](https://linear.app/playportdev/issue/PLA-76) | Partial; pins moved/held or superseded, alignment and Portal 2 follow-ups remain. |
| [Release simplification](2026-10-05-release-simplification.md) | [PLA-73](https://linear.app/playportdev/issue/PLA-73) | Open; shared filtering and release checks remain; old decision number is taken. |
| [KosmicKrisp default](2026-10-06-kosmickrisp-default.md) | [PLA-77](https://linear.app/playportdev/issue/PLA-77) | Open/partial; full i386 set and DXMT retirement are not done. |
| [Madeira audit](2026-10-06-madeira-audit.md) | [PLA-75](https://linear.app/playportdev/issue/PLA-75) | Partial; pure-x64 pool copies and NSI tables addressed, other dives remain. |
| [PC import, GOG and Epic](2026-10-06-pc-import-gog-epic.md) | [PLA-80](https://linear.app/playportdev/issue/PLA-80); implementation PLA-14/15/16 | Implementation landed, but required phone checks and deferred work remain; closed implementation issues are not the remaining-work tracker. |
| [Vulkan performance](2026-10-06-vulkan-performance.md) | [PLA-72](https://linear.app/playportdev/issue/PLA-72) | Paused 2026-10-08, merged to `main`: six fixes kept (present path, tiler off, no present wait, compression-keeping texture usage, PLA-93 freeze deferral); not at exit. Handoff: the plan's last Progress entry. |
| [Reported games without owning them](2026-10-07-compat-testing-without-owning.md) | [PLA-12](https://linear.app/playportdev/issue/PLA-12), with its existing game/feature issues | Open; reuse the existing umbrella, no duplicate. |
| [Runtime DLL coverage](2026-10-07-runtime-dll-coverage.md) | [PLA-78](https://linear.app/playportdev/issue/PLA-78) | Staging done (decisions 0070 and 0071: every Wine module Proton ships); WIC's run-time registration seeded (0072); Proton's own components wait for the owner; census, catalogue and UI warnings are optional later work. |
| [Compatibility speedrun](2026-10-08-compat-speedrun.md) | [PLA-112](https://linear.app/playportdev/issue/PLA-112) | In progress on branch `dev/pla-152-gdi-windows` (unmerged); DirectX setup skip done (0086); OpenGL through Zink done (0087–0089: x86-64 and i386; Counter-Strike, Half-Life, NWN:EE play); PLA-115 parked (diagnosis only); PLA-161 done (mesa 0030, Cat Mail on Vulkan); sweep of every issue filed in the runs (owner): PLA-182 done (GOG redists, BGEE to menu), PLA-193 done (narrowing reverted, PLA-195); PLA-177 done (wine-unix 0030); PLA-164 done (host faults end the title, not the app); PLA-120/117 done (dxdiag, lock cycle); PLA-199 done, MKKE past dxdiag; PLA-202 done (fex 0025, MKKE playable); PLA-118 done (not the cause; FM2022 USD hang fixed); PLA-203 done (wine-pe 0038: FEX notifications were dropped); PLA-205/201 done (alias release, i386 flush); PLA-192/123 done (shared install folders, compat keys merged); Zink follow-ups done (mesa 0031–0033); PLA-209 done (decision 0094); PLA-208 done (decision 0095); PLA-212 done (decision 0096); PLA-197 done (decision 0097); PLA-186 done (mesa 0035); 10a done (decision 0098); 10b done (PLA-220 fixed, PLA-173 evidence, PLA-200 in part); 10c done (PLA-224 fixed, PLA-223 fault and hang fixed; killed busy threads still run on); 10d done (PLA-222 fixed for TerminateThread, PLA-226 fixed, PLA-225 not changed); 10e done (PLA-227 `pp ui --wait` counts from JIT ready); 10f done (PLA-228, fex 0030); 11 done (decision 0099: GOG support setups; NWN Diamond still blocked on i386 Inno Setup under FEX; Quake II generation-1 design; PLA-121 reproduced; JWE/FM2022 past their anti-tamper); 11b done (PLA-235 fex 0031, PLA-233 ANSI code pages; NWN Diamond past its setup, black at its intro); decision 0100 (Vulkan is the direction); 11c done (PLA-41 JWE: wine-unix 0033 wait log, idle job loop, not fixed); 11d done (decision 0101: PLA-232 shared textures on Vulkan through Metal heaps; Valheim still black, Unity asks for an unshared texture's handle); 11d2 done (decision 0102: PLA-239 an unshared texture shared on request; Valheim's intro plays on Vulkan); 11e done (PLA-236: guest windows end at the first frame shown; NWN Diamond's Bink intro and menu show); 11f done (PLA-238: madeira-unix 0132, the fault-loop breaker no longer kills JWE's sound thread; JWE now ends `c000001d` in its protected code); 11g done (PLA-242: split-lock logging cut, wine-pe 0040, fex 0032, madeira-unix 0133; JWE ends at the same RIP sooner); 11h done (PLA-241: fex 0033, FEX traps code by host page; JWE reaches its first frame, then its WatchDog); 11i done (PLA-243: fex 0034, strict split locks by default on iOS; no lost updates); 11j done (PLA-99: SteamStub 3.1 x86 and encrypted code removed; host only, no title owned); 11k done (decision 0103: GOG script registry values, `.msi` redistributables through msiexec); 11l done (PLA-245: fex 0035, wine-pe 0041, madeira-unix 0134, guest thread context for x86-64); 11m done (PLA-244: fex 0036, split locks without a fault, 15–18 ns); 11n done (PLA-251, decision 0104: GOG command lines as Galaxy passes them; BioShock's crash is a failed small shared-section map); 11n2 done (PLA-307: madeira-unix 0135, small writable i386 section views map; BioShock to its menu); 11o done (PLA-252: the session root answers messages sent to the desktop window; TRA to its setup dialog and intro); 11p folded into the library A–Z; 11q done (PLA-302, PLA-304, PLA-305: fex 0037, wine-pe 0042, wine-unix 0036, madeira-unix 0136–0137, i386 thread context and a fresh capture at each suspension; PLA-303 not fixed); running 12 (tooling: PLA-156, 124, 119, 219), then 14–14c, and 15 (the High-priority backlog); then the library A–Z from 9 Years of Shadows. |
| [On-screen controls, mapping and gyro](2026-10-09-touch-controls.md) | [PLA-13](https://linear.app/playportdev/issue/PLA-13) | Written 2026-10-09, not started; design in `docs/design/2026-10-09-touch-controls/`; four proposed answers to confirm. |
| [Steam client bridge](2026-10-12-steamclient-bridge.md) | [PLA-253](https://linear.app/playportdev/issue/PLA-253); phases PLA-255–266 | Draft 2026-10-12, not started; option B (the game's own `steam_api`, a guest `steamclient` forwarding to the Swift client); owner decisions D1–D10 open |
| [Publisher launchers](2026-10-12-launchers.md) | [PLA-254](https://linear.app/playportdev/issue/PLA-254); PLA-267–278, 291–299 | Draft 2026-10-12, not started; Chromium hosting, then Rockstar, Ubisoft, EA, Battle.net; owner decisions open |
| [Red Dead Redemption 2](2026-10-12-rdr2.md) | [PLA-254](https://linear.app/playportdev/issue/PLA-254); PLA-268, 269, 276, 279, 300, 301 | Feasibility draft 2026-10-12; Story mode depends on donor memory (PLA-216); Online not pursued |

Status here records the review, not live issue state. Linear is authoritative for
what to do next. Overlapping performance/alignment/audit plans must share findings,
not schedule the same fix independently.

## Build-area drafts reviewed

- **KosmicKrisp FL12_2:** unfinished and distinct from backend migration. Promoted
  the research plan above, removed private inventory/storage/pricing assumptions,
  and linked PLA-81 to PLA-77/PLA-76/PLA-72. The original Mesa move is already
  reflected in current pins; capability proposals still need validation.
- **PC import, GOG and Epic:** removed the stale `.work/plans/` duplicate. The
  version here contains later owner decisions (Local imports stay Local, no
  installers) and progress/evidence. PLA-80 already tracks remaining work; no
  duplicate issue or discarded requirement was revived.

Neither build-area draft was a completed execution run. Their local copies were
removed after review; raw private FL12_2 research remains in `.work/scratch/fl122/`.

## Completed runs removed

- **Guest VA exhaustion:** the fix and final regression gates passed; see
  [outcome and evidence](finished.md#guest-address-space-exhaustion). Separate
  cache and VA-layout work stays in PLA-76/PLA-75.
- **Store-game sign-in:** the implementation run ended and its open follow-ups
  already have issues; see [outcome and evidence](finished.md#games-sign-in-to-their-store).
  Release was not completed: [PLA-74](https://linear.app/playportdev/issue/PLA-74)
  now tracks that handoff.

- **OpenGL through Zink:** chunks 1a–5 done 2026-10-11 (decisions 0087–0089); see
  [outcome and evidence](finished.md#opengl-through-zink). Remaining work is in
  PLA-181, PLA-184, PLA-186, PLA-189 and PLA-164.

[Finished plans](finished.md) is a historical outcome/link index, not an active
plan. Full removed plans remain in git history. Keep partial plans until mandatory
gates pass or remaining scope is explicitly handed to open linked issues. Removing
a completed run must not erase its evidence or imply its follow-ups passed.

This review changed documentation and tracking only: no plan implementation,
build, phone play, release draft or publication was performed.

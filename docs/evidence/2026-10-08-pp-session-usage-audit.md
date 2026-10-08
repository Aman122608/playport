# `pp` session-usage audit

**Linear:** [PLA-94](https://linear.app/playportdev/issue/PLA-94)

Host-only audit of source at `615e6e0`, in the authorized
`session-docs-audit` worktree. No build, install, phone play or performance
benchmark was run; there is no new IPA. Recommendations below are **not
implemented**. Earlier documentation fixes under
[PLA-84](https://linear.app/playportdev/issue/PLA-84) are already in this branch.

## Scope and confidence

Inventoried 214 top-level Playport Pi session JSONL files, including historical
case variants and side worktrees, with session starts from September 24 through
October 8 before 07:00 UTC. Searched recorded shell-tool arguments and their
results, then inspected selected failure/retry sequences and current code.
180 files contained shell records mentioning `pp`. This is a discovery count,
**not** 180 exhaustively reviewed sessions or a count of executed commands:
arguments also contain grep patterns, scripts being written, loops and quoted
commands. Nested child-session directories were not exhaustively scanned.

Private extracts and probes are under `.work/audit/pp-usage/`; the earlier
six-session extracts remain under `.work/audit/`. Raw transcripts, credentials,
account inventories and absolute workstation paths are not published here.
Session references below give the filename's timestamp prefix and JSONL line,
so the owner can locate the original without publishing private session content.

Timing sources are distinguished: foreground call/result elapsed time,
background task `endTime - startTime`, or build-stage log timestamps. A
background **launch receipt is not execution duration**. These are historical
samples, not a benchmark or a total-hours-wasted estimate; concurrent work,
necessary regressions, changed inputs and notification delays prevent that.
No model ranking is justified by this task-mixed sample.

## Most consequential observations

| Evidence | Observed cost / problem | Better next action |
| --- | --- | --- |
| Oct 6 `14-46-17-604`, L603/L627; subsequent user correction | Two ten-play batches took 1,316.811 + 1,446.262 s (**46m03s**). They did not establish gameplay: successful starts stayed in the cinematic. | Verify the route/scene once before repeating; first frame and FPS do not certify gameplay. |
| Oct 6 `12-29-54-633`, L120–132 | Four `pp test --quick` calls, about **45 s each**, exposed the same missing EpicClient fixture before its fix. Output was repeatedly discarded with tail/grep. | Keep the first full log, inspect its failure, run that test after fixing it, then the relevant integration suite. Three pre-fix reruns cost about **135 s** without testing a fix. |
| Oct 7 `23-18-18-056`, task records L109/L157 | Two doc/plan chunks each ran a broad quick suite: **46.957 / 46.986 s**. | Check links/contracts and staged names/secrets per doc chunk. Run broader tests when those docs participate in executable contracts, not automatically for every prose change. |
| Oct 6 `10-30-00-879`, L117 | `pp check` took **226.27 s** call-to-result, but its final line reported **18 s** for the app. Dependencies and notices came first; an earlier warning did explain that. | Report total and phase times together. `check` is an unsigned app build, not a cheap host check. |
| Oct 7 `20-32-03-795`, build logs L88/L116 | Two diagnostic IPAs took about **123 / 124 s**. Each spent about **30 s in notices**, then **19–20 s in app**, about **4 s in verify**. | Profile notice collection/selection/staging before optimizing. The whole 123 s is not needless work, nor is all 30 s proven removable. |
| Oct 7 `20-32-03-795`, L139 onward | `--settings … --action play:…` played before applying settings; the attempt was canceled and rebuilt as a command sequence. Several speculative dialog probes still produced no Static body. | Inspect `UI_ACTIONS` via UI dry-run, use `--settings` with `--play`, and establish a relevant host/static repro before spending more device runs. PLA-59 remains the actual missing gate. |
| Oct 4 `19-43-45-950`, L125; current help reproduction | Fractional `--screenshot-after` was rejected twice. Today `pp ui --help` prints two complete usage blocks, **33,454 characters**. | Parse once; separate concise CLI reference from extended documentation. Avoid truncating help so aggressively that relevant ordering rules disappear. |

The cinematic runs were not wholly valueless: they exercised startup/video and
found failures. The waste was treating them as the intended gameplay evidence
and expanding the batch before checking its route. A pad at `first-frame+45`
replaced a 53.5 s new-game script started at `+25`; it did not queue behind it.
The corrected route and its limits are already in
[DEVICE.md](../DEVICE.md#driving-a-title) and the
[baseline correction](2026-10-06-vulkan-perf-baseline.md#correction-the-60-s-stability-plays-ended-in-the-opening-cinematic).

## Current misleading or unsafe contracts

### 1. Successful command is not necessarily a successful checkpoint

`tools/phonelib.py:watch_launch` stops on a terminal event **or** the requested
`--until`. `tools/ui.py:main` uses `title_ok` for a clean `title-done`, even if
`first-frame+10` was never reached. This matches the documented OR-stop rule,
but makes `ok: true` unsafe as the sole assertion for a smoke test. The existing
`title_failed` restart guard fixes nonzero exits, not early clean completion.

A mocked stream containing only a clean title exit at one second produced
`watch_stop=done`, `title_ok=true`, with no first-frame event. This is a
host-confirmed contract gap, **not an observed phone false pass in this audit**.
Expose the requested predicate, observed milestones and actual stop reason;
provide strict checkpoint assertions without breaking intentional quit flows.
Track under [PLA-102](https://linear.app/playportdev/issue/PLA-102).

Related timing surprises:

- UI watch receives **`--wait + 180`**, unconditionally; this is not measured JIT
  time. `--wait 120` permits a 300 s watch even if JIT finishes immediately.
- Perf passes `--wait = --secs + 300`, so its UI watch budget is
  **`--secs + 480`**. `--secs 120` can spend 600 s waiting for a missing frame,
  apart from lock, setup, cooling and cleanup. `--secs` is a measurement duration,
  not an end-to-end timeout.
- UI's `main_recorded` updates `last-play.json` based on requested argv in a
  `finally`, including valid play requests that failed before launching. That
  can impose later cooldown for a play that did not happen.

Separate startup, measurement and total budgets, and record actual phase
completion. Do not simply shorten legitimate JIT/setup waits globally.

### 2. Dry/offline perf paths can touch live-run resources

Confirmed with process/device methods mocked, without touching a real monitor:

| Entry point at this snapshot | Observed call | Consequence of the real implementation |
| --- | --- | --- |
| `perf.run_main(['--dry-run', …])` | `stop_follow()` once | `stop_follow` signals the stored thermal follower's process group and unlinks its PID file, even though dry-run bypasses the device lock. |
| `perf.main(['--profile', DIR])` | `device_lock()` once | Reading an existing profile can wait behind the phone; lock cleanup also has unattended behavior. |

`perf.main` takes the lock before argument parsing for live mode, so an invalid
option can wait for the phone before reporting a local syntax error. Parse and
validate first, classify the mode, then acquire only the required resources.
Regression-test **absence of side effects**, not just successful output.
[PLA-101](https://linear.app/playportdev/issue/PLA-101) is highest-priority tooling
work alongside checkpoint correctness.

### 3. Test output encourages costly rediscovery

`pp test` streams tests but does not retain a unified log/failure index itself.
A tail may hide the failing Python test behind later C/Swift output; piping
without the producer's exit status can also make a failure look successful.
The Oct 6 four-run sequence demonstrates both the diagnostic-loss risk and the
cost of rerunning the whole suite to ask for a different excerpt.

`--quick` only omits Swift; in the observed runs `tools/tests` took about 30 s
and the WoW64 C group another 11 s. It is not a focused test mode. Swift already
has `--swift PKG --filter X`; Python/C need similarly discoverable selection.
A mocked `cmd_test(['--quik'])` dispatched the suite, including all six Swift
packages, and returned success when its mocked tests passed: unknown arguments
are not rejected on the non-Swift path.

Add retained logs, a concise final failure list and structured scope/timing/skip
metadata. A focused pass must never claim the full suite passed. Preserve the
actual exit code, including through agent wrappers. Track under
[PLA-100](https://linear.app/playportdev/issue/PLA-100).

### 4. Performance numbers need evidence-quality checks

`tools/perf.py` records requested settings, scripts and measurements, but those
are not proof of the scene or effective settings. It schedules pad pushes
without overlap validation. A summary's existence does not establish full
window coverage, HUD/GPU availability or gameplay. Existing
[PLA-82](https://linear.app/playportdev/issue/PLA-82) records an actual lost-HUD
repeat; do not confuse that open issue with the earlier cooling-session fix.

[PLA-105](https://linear.app/playportdev/issue/PLA-105): preview script durations,
hashes and overlaps; warn rather than ban intentional replacement; carry frame
source, coverage, requested/observed settings and missing data into comparisons.
Make scene validation an explicit evidence reference, not an inference from FPS.
Reuse existing `--analyze` / `--compare --window` instead of replaying the phone
to calculate another statistic. Diagnostics/capture must remain distinguished
from comparable performance runs.

## Documentation: fixed already versus still wrong

PLA-84 already corrected action ordering, smoke-versus-gameplay guidance,
Vulkan GPU-capture availability in the reference, the unconditional desktop
success/failure inference, build/sync drift and build-lock semantics. The
historical “second build waits” description is superseded: the build uses
`flock -n`; the phone lock waits. Do not reopen these as new fixes.

Remaining drift at this snapshot, tracked by
[PLA-104](https://linear.app/playportdev/issue/PLA-104):

- `perf --gpu-capture` option help still says **DXMT**, although Vulkan capture
  is implemented and documented in DEVICE/GPU-DEBUGGING.
- `pp test --help` lists only three Swift packages; `checks.SWIFT_PACKAGES`
  contains six, including ContentKit, GOGClient and EpicClient.
- UI help says dry-run will “touch nothing”; it creates local run/argv/events
  files. “No phone access” is accurate for UI; it must not be generalized to
  perf's currently unsafe cleanup path.
- DEVICE's cooldown bullet omits the last UI play and the bounded
  `--cool-max` fallback; the implementation considers both perf and UI play.
- The “up to 180 s” JIT wording obscures the fixed addition to UI's deadline.
- `pp names` and `pp secrets` scan the **index**, not unstaged/new text. A clean
  scan before staging is not evidence that the new document was scanned.
- `ui.main_recorded` reparses in `finally` via `played`, duplicating help and
  parse errors. This is a code defect, not just verbose documentation.

Keep canonical facts close to their parsers/constants and test key CLI/doc
contracts with host tests. Do not solve drift by pasting another complete
command reference into AGENTS.md. Existing historical decisions/evidence must
remain historical, not be rewritten as today's recipes.

## Build and debugging cost priorities

1. **Stop invalid experiments early.** Check a scene before a campaign; use
   bounded game-start marks for known no-frame investigations. For hidden
   windows use retained `[win-pos]`, `[win-text]` and `[msgbox]` evidence, inspect
   the executable/arguments on the host, then run the relevant phone gate.
   Repeated arbitrary-title probes left [PLA-59](https://linear.app/playportdev/issue/PLA-59)
   unproved; more launches are not automatically more evidence.
2. **Preserve diagnostics and narrow iteration.** Keep full output and actual
   producer status on the first attempt. Use focused host tests while editing,
   then relevant integrated/full gates before accepting the coherent change.
   Check Swift package logic without an IPA when possible. Docs-only work does
   not require an app build; executable documentation contracts still need tests.
3. **Plan the build once, avoid redundant app compiles.** In main,
   `pp build --plan` shows invalidation. If the next required step is an IPA,
   consider going directly to the build after focused checks rather than always
   doing `check`, `build`, then an install that builds again. Reuse the exact
   verified IPA with `install --no-build --ipa …` where appropriate. Do not
   weaken provenance, signing or verification to save compilation time.
4. **Measure notices before caching them.** `build/pipeline:digest_inputs`
   depends on all tree/stage stamps and `app/artifacts.tsv` for notices;
   `stage_notices` reconstructs collection/selection outputs. The observed
   ~30 s merits profiling and per-component, content-based reuse research
   ([PLA-103](https://linear.app/playportdev/issue/PLA-103)), not removal of legal
   gates. Prove invalidation and identical outputs before claiming a speedup.
5. **Preserve controls and one-phone ownership.** Existing
   [PLA-92](https://linear.app/playportdev/issue/PLA-92) records default retention
   pruning an installed A/B control, forcing a rebuild. Keep controls explicitly
   until comparisons finish. Historical September sessions also repeatedly
   queued behind other phone sessions; another driver or borrowed lock marker
   is not a remedy. Use one real lock session for sequences, bounded lock waits
   when a deadline matters, and independent host work while waiting. Keep
   worktrees host-only; never share mutable build state to save setup time.

These changes target **information gained per run**, before chasing seconds off
a correct build. The strongest measured avoidable costs are the 46-minute
mis-targeted campaign and repeated ~45-second diagnostic rediscovery. Notice
caching and compilation reuse are hypotheses requiring new measurements.

## Validation and handoff

- Scanner completed with exit 0; cutoff-adjusted inventory is in
  `.work/audit/pp-usage/summary-cutoff.json`. Raw matching counts were not treated
  as executions or used to estimate total waste.
- Four isolated host probes passed their baseline assertions: perf dry-run
  reaches cleanup, offline profile takes the lock, unknown test flag dispatches
  Swift, early clean title completion can precede the checkpoint. These confirm
  the **reported current defects/contracts**, not fixes. Device/process calls
  were mocked. Probe script/results remain in ignored audit scratch.
- `./pp ui --help` reproduced the duplicate usage without phone access.
- Publication checks: staged name/secret gates and diff whitespace check; no
  broad test-suite rerun is needed for this evidence-only report.
- No main-checkout edits, build/device state changes, push or integration.
  Device/runtime gates for future implementations remain unrun. New work is in
  PLA-100–105, with existing PLA-59/82/92 reused rather than duplicated.

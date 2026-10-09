# JIT: require a live tracer before universal breakpoints

**Linear:** [PLA-147](https://linear.app/playportdev/issue/PLA-147), child of
[PLA-11](https://linear.app/playportdev/issue/PLA-11).
Hollow Knight result: [PLA-42](https://linear.app/playportdev/issue/PLA-42).

**Branch:** `dev/pla-147-detached-debugger`.
**Phone:** iPhone18,4, iOS 27.0, TXM present.

## Change

Swift method selection and C's JIT-pool acquire share
`wine_host_jit_debugger_state` (`app/Sources/WineHost/jit_debugger.c`). It
requires both `csops`'s `CS_DEBUGGED` and `sysctl(KERN_PROC_PID)`'s `P_TRACED`.
Failed or short queries fail closed. The C prepare and detach sites each
recheck immediately before `brk #0xf00d`.

Another app with stale signing status fails before pool acquisition with
script/relaunch guidance; Built-in and StikDebug can request a fresh attach
instead of skipping their helper. Timeout and failed breakpoint gates also
get actionable messages. The LiveContainer message names assigning
`universal.js` to **LiveContainer**, not Playport. Player-facing gate reasons
carry a `JIT setup: ` marker through `LaunchMessage`; the release screen
preserves them instead of replacing them with generic LocalDevVPN advice.
The view strips the marker and omits its redundant generic fix. Raw helper
errors retain their existing release filtering.

This addresses the known attach-and-detach crash, not the original Portal 2
Vulkan black-screen report. A live tracer alone does not establish protocol
support. It can also detach after the check and before the breakpoint: no
race-free or wrong-script detection is claimed.

## Checks

- `./pp test`: passed, including 545 Python tests, the new C debugger decision
  test, all Swift packages and the new PlayportKit guidance test.
- `./pp check`: dev app compiles unsigned.
- `./pp names`, `./pp secrets`, `git diff --cached --check`: clean with the
  implementation staged.
- Dev build: **80 IPA checks passed**, installed by an in-place upgrade.
  No runtime artifact records changed.

## Built-in JIT regression

**IPA:** `.work/out/20261009-092740-392fab9c/Playport-26.5-392fab9c.ipa` (dev).
**SHA256:** `392fab9c65ff4bbddc1180632f9a67d1b96012b1c998963985f95bb45b08a2e8`.

`pp ui` drove `set:jit.method=builtIn`, then Hollow Knight (`app-367520`)
through `first-frame+10`. Result: `ok=true`, exit 0, run
`.work/pla-147/builtin-final`. The log shows the built-in helper attaches, the
prepare succeeds (1.01 s), and detach completes. The 512 MiB acquire returns
0 after **2.58 s**; runtime **+3.88 s**, game **+4.09 s**, first frame
**+9.28 s**. Pool self-check passes. The stop screenshot shows the game's
main menu; it remains private at `.work/pla-147/builtin-final/screen-stop.png`.

An earlier version, before preserving the release error-screen guidance,
also passed: dev IPA
`c397c5f2634a5e905ed852be9fe91d7d2f050f510d4eae56c54cdeb0dbfee78a`,
`.work/pla-147/builtin`, acquire 2.48 s, first frame +10.13 s.

This is a launch/main-menu smoke check, not sustained gameplay or a new
performance measurement. Hollow Knight stays `works` in the public
compatibility list; the latest smoke is recorded on PLA-42.

## LiveContainer test release

**Source IPA:**
`.work/out/20261009-092924-release-unsigned-48a383c1/Playport-26.5-release-unsigned-48a383c1.ipa`.
**SHA256:** `48a383c1a0c23131c75f796928a934be19226c5797ba0e46662b4ae7022a0ab7`.
Release build: **78 IPA checks passed**. This identifies the unsigned import
source, not the application LiveContainer subsequently signs.

The first staged IPA (`70205149…`) predates the release guidance-preservation
fix and is superseded. The handoff uses **LiveContainer's Documents**, as
`Playport-PLA-147.ipa`, for import through LiveContainer's UI. An initial
copy into standalone Playport's Documents was not accessible in Files:
Playport does not enable file sharing. LiveContainer does.

## LiveContainer phone checks (owner driven)

The owner imported the corrected release over the existing Playport guest
in LiveContainer 3.8.0 and drove both checks by hand, with StikDebug 3.1.13.
JIT method: **Another app**. Title: the existing Shogun Showdown GOG install,
DXMT, 512 MiB pool, 6144 MB memory limit. The workstation collected evidence
only; no release UI driver or automated release qualification was used.

### Missing script

The owner confirmed the launch stayed alive and displayed instructions to
assign **universal.js to LiveContainer in StikDebug**. The app log has two
refusals (07:34:56Z and 07:36:20Z): `CS_DEBUGGED is set but no live tracer is
attached`, then the player-facing `JIT setup:` reason and `launch=failed`.
Activation is **0.00 s**, with no pool reservation/prepare or acquire.
The generic `runtime started` timing mark is emitted even for this failed
start; it is not evidence that Wine started. The failure result and absence
of pool acquisition establish the refusal.

Raw log: `.work/pla-147/lc-negative/playport-1.log` (appended sessions,
including older runs). Its screenshot caught the opening splash, not the
failure screen; the visible error wording is the owner's observation, also
confirmed by the logged reason. No failure-screen screenshot is claimed.

### universal.js assigned

The owner confirmed Shogun Showdown still works after assigning universal.js
to LiveContainer and relaunching with JIT. The final collected log
`.work/pla-147/lc-positive/playport-1.log` independently confirms the play
at **07:39:30Z**:

- `jit: method external (in LiveContainer)`; live debugger already attached;
- prepare succeeds in **1.53 s**; RX/RW aliases and debugger detach succeed;
- `jit: acquire(512 MiB) -> 0 after 1.64 s`; pool self-check passes;
- runtime **+2.21 s**, game **+2.49 s**, first frame **+9.19 s**.

An earlier changed-build play at **07:35:41Z**, between the two new-gate
refusals, also passed: prepare 1.66 s, acquire 1.77 s, runtime +2.25 s,
game +2.52 s, first frame +9.18 s. The final screenshot shows the game's
page with the accepted LiveContainer close/relaunch warning after the play,
not the game screen; it remains private at
`.work/pla-147/lc-positive/screen.png`.

Thus the new gate preserves a live universal-protocol debugger and refuses
the stale signing flag without issuing its fatal breakpoint. This is a
launch check, not proof of sustained gameplay, controller input or quit/
relaunch behavior. The owner had no controller connected during these plays.
Shogun Showdown remains `works` (launch/title-screen evidence).

The previous crash and workaround are in
[the earlier evidence](2026-10-09-livecontainer-universal-jit.md). The original
Portal 2/Vulkan black-screen report remains open in PLA-11; this fix does not
establish its cause.

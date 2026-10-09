# LiveContainer: Portal 2 reaches its menu on Vulkan

**Linear:** [PLA-11](https://linear.app/playportdev/issue/PLA-11),
[PLA-26](https://linear.app/playportdev/issue/PLA-26).
**Date:** 2026-10-09. **Branch:** `dev/pla-11-livecontainer-game-loads-to-a-black-screen`.
**Phone:** iPhone18,4, iOS 27.0, TXM present; LiveContainer 3.8.0,
StikDebug 3.1.13 (the existing test setup).

## Build and procedure

The owner drove the existing PLA-147 release test guest in LiveContainer,
using its UI to launch Portal 2. Requested setup: universal.js assigned to
LiveContainer in StikDebug, Launch with JIT, Another app JIT method, Vulkan,
and default screen settings. The workstation collected files and screenshots
only; it did not drive the release app or provide JIT.

**Unsigned import source:**
`.work/out/20261009-092924-release-unsigned-48a383c1/Playport-26.5-release-unsigned-48a383c1.ipa`.
**SHA256:** `48a383c1a0c23131c75f796928a934be19226c5797ba0e46662b4ae7022a0ab7`.
This is the source IPA, not LiveContainer's subsequently signed application.
The log's running-image UUID matches the final PLA-147 positive check
(`.work/pla-147/lc-positive/playport-1.log`). Prior build verification and
regression checks are recorded in
[the PLA-147 evidence](2026-10-09-detached-debugger-gate.md); no new app build
or runtime change was made for this check.

## Result

The owner reported **“It reached menu.”** The collected screenshot independently
shows Portal 2's rendered main menu, with Play Single Player highlighted.
The original loading-then-black-screen symptom did not reproduce here.

The successful play begins at **08:10:16Z** in the app log:

- `jit: method external (in LiveContainer)`; debugger already attached.
- `jit: acquire(512 MiB) -> 0 after 1.99 s`; pool self-check passes.
- Runtime **+5.01 s**, game **+5.35 s**, first frame **+11.36 s**.
- `C:\Games\Portal 2\portal2.exe`, Steam app ID **620**, Vulkan.
- Screen **1564x720**, phone aspect, **60 FPS cap**, **6144 MB** memory limit.
- FEX disk cache off; pool exhaustion counters zero at the logged snapshots.
- The setup line reports **no controller**, despite the requested connection.
  Controller input was not verified. Thermal state is serious at launch,
  then nominal; this is not a performance comparison.

The same collected log also contains an earlier attempt at **08:09:44Z**:
`CS_DEBUGGED is set but no live tracer is attached`, followed by the
PLA-147 script/relaunch guidance and `launch=failed`, activation **0.00 s**.
No pool is acquired for that attempt. Its generic `runtime started` timing
mark is not evidence that Wine started. The owner did not separately describe
that attempt's on-screen message during this check.

## Collection and limits

Private files: `.work/pla-11/lc-portal2/playport.log`, `screen.png`, and
`cmd/`. Screenshots contain game artwork and are not committed. Collection
completed with exit 0. Two preceding collection attempts failed because the
old guest-container path no longer existed and root enumeration encountered
an inaccessible metadata file. Listing Documents located the current guest;
no container was modified, no app was installed or uninstalled.

This establishes launch/menu rendering on our phone and this test build,
not gameplay, controller input, quit/relaunch, or a fix for the original
reporter's iPhone 16e. No same-build standalone A/B was needed to classify
this successful launch, and none was run in this session.

**PLA-11 stays open.** LiveContainer is not sufficient by itself to cause
Portal 2's black screen. The original Portal 2 and tPimple failures still
need a current-build reporter log and setup details; no such report was
located in the inspected `.work/reports/` inventory. The missing-script
crash is separately addressed by PLA-147; it is not established as the cause
of those black screens. Portal 2 remains `works` in the public compatibility
list; its latest check is recorded in PLA-26.

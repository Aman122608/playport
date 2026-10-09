# LiveContainer: Shogun Showdown launches with universal.js

**Linear:** [PLA-11](https://linear.app/playportdev/issue/PLA-11),
[PLA-48](https://linear.app/playportdev/issue/PLA-48).
Detached-debugger follow-up: [PLA-147](https://linear.app/playportdev/issue/PLA-147).

**Phone:** iPhone18,4, iOS 27.0, TXM present.
**App:** Playport 0.4.0 release, imported into LiveContainer 3.8.0;
StikDebug 3.1.13 enables JIT. Source IPA sha256
`dbeed0312025637a52c4c9493d6ec5d37008016cc5c9edeba5165ca8883df9c3`
(`.work/releases/v0.4.0/Playport-0.4.0.ipa`, hash checked against the release
manifest). This identifies the imported release, not LiveContainer's subsequently
signed application or the separately installed standalone Playport.

The owner drove the release UI by hand; workstation operations only collected
logs. LiveContainer and StikDebug use reused App IDs on this phone because the
free-team App ID allowance was exhausted. Automatic script selection under the
normal LiveContainer bundle ID was not checked.

## Without the script (2026-10-08)

GOG sign-in, the library and installation work inside LiveContainer. Shogun
Showdown (GOG 1104084973, build 1.0.2.1a, 208 files, about 433 MB) crashes at
Play before its runtime starts. Playport says `jit: method external (in
LiveContainer)` and `debugger already attached`, then stops after the JIT pool
reservation.

StikDebug's log shows `vAttach;2d58` followed by `$D` about one second later.
The crash report shows `EXC_BREAKPOINT / SIGTRAP`, with the triggered thread in
`jit26_prepare_region`, called from `JitProvider.acquire` and
`WineHostRuntime.start`. The binary pulled from LiveContainer was symbolized
with llvm-symbolizer to identify those frames.

Both Swift's `debuggerAttached` and C's `debugger_attached()` use `CS_DEBUGGED`.
The attach-and-detach leaves that code-signing flag set: it is not evidence of
a currently attached debugger. Playport consequently issues an unserved
`brk #0xf00d`. The missing-script setup needs an actionable failure instead of
this crash (PLA-147); no code fix is claimed here.

## With universal.js (2026-10-09)

The owner assigned **universal.js** to LiveContainer in StikDebug, relaunched
Playport with JIT, and pressed Play on the same Shogun Showdown install, using
**DXMT**. The owner reported that the game launched fine. The collected app
log independently confirms:

- `jit: method external (in LiveContainer)`;
- `JIT pool: prepare(NULL, 0x20000000)` succeeds in 1.77 s;
- the pool has RX and RW aliases, then the debugger detaches;
- `jit: acquire(512 MiB) -> 0 after 1.86 s`;
- runtime started at **+2.20 s**, game started at **+2.32 s**;
- **first frame at +19.73 s**; subsequent log entries include audio buffers.

Thus universal.js resolves this launch-time crash on our setup without a
Playport update. The public compatibility status for Shogun Showdown remains
`works`; this run does not extend the previous title-screen result to verified
gameplay.

## Limits and raw evidence

Portal 2, Vulkan, sustained gameplay, controller input, quitting and relaunching
after a hung game were not checked in this run. The original Portal 2
black-screen report remains open in PLA-11; a missing script is not established
as its cause. This is a manually observed release run, not an automated release
qualification or a new build.

Raw files remain ignored/private:

- `.work/lc/pull2/playport.log`: failed launch;
- `.work/lc/crashes/LiveContainer-2026-10-08-175231.ips`: crash;
- `.work/lc/stik-pull/idevice_log.txt`: plain attach/detach;
- `.work/lc/ipa-x/Playport`: binary pulled for symbolization;
- `.work/lc/pull3/playport.log`: failed launch followed by successful launch;
- `.work/lc/syslog3.txt`: partial capture of the successful session.

No screenshots or pairing/signing material are published.

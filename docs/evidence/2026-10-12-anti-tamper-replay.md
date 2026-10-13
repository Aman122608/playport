# Evidence: JWE and FM2022 no longer fail in their anti-tamper on today's build (PLA-122)

**Date:** 2026-10-12. **Kind:** phone runs (dev app). **Linear:**
[PLA-122](https://linear.app/playportdev/issue/PLA-122), titles
[PLA-41](https://linear.app/playportdev/issue/PLA-41) (Jurassic World Evolution, Epic) and
[PLA-114](https://linear.app/playportdev/issue/PLA-114) (Football Manager 2022, Epic). The owner
was away; the conservative option was taken at each choice. Run directories are under
`$PLAYPORT_BUILD/ui-runs/`. **IPAs:** `Playport-26.5-4b3b3b50.ipa` (dev, sha256
`4b3b3b507cdb992ca9b2b432be13914e265d90a81008098ad959a1281005a5fa`, commit d331108 with fex 0030;
the replays) and `Playport-26.5-12062a00.ipa` (dev, sha256
`12062a00f1624405dc4fa8e529eab303ed4b29ef6036e56bcafb4428702c8bbd`; the JWE A/B runs, with a
GOG-only change over it).

Sweep 2.6 left both titles dying `c0000005` in their anti-tamper, FM2022 after about 5 s at RIP
`0x159782e84`, JWE after 7.7 s at `0x1488a3cf6`
([jwe-commit-probe](2026-10-09-jwe-commit-probe.md), sweep 2.6's report). Sweeps 2.10a to 2.10f
changed the machine registry values (decision 0098), FEX's CPUID brand string (PLA-220, sweep
2.10b), thread termination and RWX page reuse (fex 0030). The sweep replayed both before tracing anything.

## Football Manager 2022: the anti-tamper asks its server, which refuses for 24 hours

Run `s11-fm22-base` (`4b3b3b50`, first frame +30 s, `--wait 90`): no fault. A first window at
+6.5 s, then the protection's own dialog, *Football Manager 2022*: "Sorry, something went wrong.
For solutions please visit:" `https://support.codefusion.technology/fm22_8a023a2/?e=88500006&l=en`.
`wininet` was loaded and its connection thread ran. That support site's script lists `88500006`
among the codes that show its FAQ text instead of a contact form, and the FAQ (its `lang/english.json`,
key `24H`) reads: "Currently your game purchase cannot be re-validated successfully, please wait 24
hours and try again."

So FM2022 now runs its protection to the online re-validation, and the server refuses it for the
day. Each change to what the protection fingerprints (0098's machine values and the CPUID brand
string of 2.10b, across many runs this week) asks for a re-validation, which plausibly used up the day's quota. No
platform check is failing that a trace could show.

## Jurassic World Evolution: the engine starts, then its WatchDog ends it

| Run | IPA, graphics | Outcome |
| --- | --- | --- |
| `s11-jwe-base` | `4b3b3b50`, its page's DXMT | `exit=0xc0000005` after 48 s, not 7.7 s |
| `s11-jwe-vk` | `12062a00`, Vulkan (session only) | the same after 47 s; screenshots black at 40 s and at the stop |
| `s11-jwe-dbg` | `12062a00`, DXMT, `env.WINEDEBUG=warn+seh` (session only) | the same after 48 s; the game's `OutputDebugString` lines |

The game now gets past its anti-tamper: its threads `Main`, `Render`, `Message Pump`, `WatchDog`,
`fTask W0`–`W3`, `fIO 0`–`7`, Wwise's, `HttpManagerThread` and `LibwebsocketsThread` start, it
creates its 1564x720 window, and its prerequisite and Epic token steps are as before. No frame is
presented. The thread samples every 20 s show `Main` and `Render` waiting in
`NtWaitForMultipleObjects` (0 % CPU) and no thread busy. At about 47 s the `WatchDog` thread runs
`mov dword [1], 0` at `JWE.exe` `0x140a6c820` (`c7 04 25 01 00 00 00 00 00 00 00 c3`), a deliberate
crash; the crash handler's debug lines follow ("Crash detected, attempting to write out dump.", a
minidump under the user's `Temp\Frontier Developments\…\CrashDumps`, 32 bytes) and it starts
`crash_reporter.exe`. The text `WatchDog` is on the faulting thread's stack.

## What this changes

The planned trace of the anti-tamper's platform reads (process information classes, debug
registers, CPUID/RDTSC, the shared user data, image and export-stub reads) is not the next step for
either title: neither fails there now, so no logging patch was built. A desktop Proton A/B would
still need an Epic launch token on the workstation, which it does not have; none was sought.

- **FM2022:** replay once after 24 hours with no change to the machine values or the CPUID brand in
  between. If the server accepts, the game should go on to its own start.
- **JWE:** find what `Main` waits on when the watchdog fires (its guest stack at the wait: a
  dev-only sample of the waiting thread's guest return addresses, or a handle-to-object log for its
  `NtWaitForMultipleObjects`); the HTTP and websocket threads suggest a network or Epic
  (online services) step worth checking first.

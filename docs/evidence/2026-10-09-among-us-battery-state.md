# Among Us: the battery query crashed in CFNumberGetValue (PLA-39)

**Date:** 2026-10-09. Phone: iPhone18,4, dev build. Title: Among Us (`app-945360`, v19s build
7489), Vulkan, 16:9. Run directories are under `$PLAYPORT_BUILD/ui-runs/`.

## Before (IPA `d638478a…`, run `20261009T035726`)

The game reaches its Privacy Policy dialog 5 s after start, then its main menu with the age
check; EOS signs in with Steam (`Platform Login is success`). Confirming a birth date ended
the game with c000001d:

    [mach_exc] sym pc=CoreFoundation`<redacted>+0x0 lr=CoreFoundation`CFNumberGetValue+0x1b4
    [mach_exc] bt[0] S1Probe`NtPowerInformation+0x4f8
    002c:err:seh:segv_handler SEGV LOOP FATAL: pc=0x19c587f94 addr=0x0 after 5 repeats, forcing thread exit
    002c:err:seh:NtRaiseException Unhandled exception code c000001d flags 0 addr 0x19c587fc8

Wine's macOS `fill_battery_state` (behind `NtPowerInformation(SystemBatteryState)`) released
the `IOPSCopyPowerSourcesInfo` blob before reading the description it owns, and read the
capacities, charging state and power source state without a NULL check. The 2026-10-07 crash
"17 s after Play" was the same fault.

## Fix (wine-unix 0024)

The blob lives until the end, every key is read through a type-checked helper with a default,
and no present internal source reports AC.

## After (IPA `dbd94bf4…`, run `20261009T040912`)

The age check is past (saved before the crash); the game opens its announcements over the main
menu (first frame 4.9 s after Play, JIT 2.4 s), online (green status, EOS
`finishing flow, login status LoggedIn`). Play › Practice › The Skeld loads the map, and the pad
moves the crewmate in the Cafeteria. No `CFNumberGetValue` fault in about 7 minutes.

Still open: EOS logs `Lws(Error): getprotobyname() failed with error 11004`. The prefix has no
`C:\windows\system32\drivers\etc` (`protocol`, `services`, `hosts`), which `ws2_32` reads;
sign-in works without it.

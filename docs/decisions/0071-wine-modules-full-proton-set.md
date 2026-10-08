# 0071: Every Wine module Proton ships, aarch64 and 16-bit included

**Status:** accepted, 2026-10-08, by the owner ("If we want to support *every* game
in the future, we need what Proton ships"; [PLA-78](https://linear.app/playportdev/issue/PLA-78)).
Supersedes three parts of [0070](0070-wine-modules-proton-parity.md): the aarch64
companion set, the 16-bit exclusion and the programs left out (`control`,
`winedbg`, `winemenubuilder`). The rest of 0070 stands: the selection comes from
`build/generated/wine-pe-*.tsv`, replacements win, no census gate.

## Decision

- **Every architecture takes its whole production set.** The arm64ec, aarch64 and
  i386 sets are every `dlls/` and `programs/` entry of the build's manifest, without
  `tests/` and without `winetest.exe` (Wine's install and Proton have none). The
  aarch64 set is no longer the WoW64 layer plus the closure of the programs' imports:
  0070's reasoning that a full set serves no guest was not tested (a `LoadLibrary`
  by name, COM by registration, an ARM64 process started from a staged program
  could each reach an aarch64 module outside the closure). It is laid out as Proton's
  own ARM64 build (`--enable-archs=arm64ec,aarch64,…`) would lay out its native side.
- **i386 keeps its 16-bit modules** (`.dll16 .drv16 .exe16 .mod16 .vxd`), as Proton's
  i386 set does. New WoW64 runs no 16-bit code; they cost 10.4 MiB and are there for
  the 32-bit programs that load or look for them.
- **`winedbg`, `control` and `winemenubuilder` are staged, with what they would do on
  the phone turned off in the registry seed** (`NEUTRALISE` in
  `app/tools/prefix-registry.py`, marked for top-up so a prefix's own value wins):
  - `HKCU\Software\Wine\WineDbg` `ShowCrashDialog=0`, exactly what Valve's
    `wine.inf` writes into Proton's prefix. kernelbase still starts
    `winedbg --auto` for an unhandled exception (Wine's and Proton's `AeDebug`
    default); winedbg logs a backtrace, detaches and the title ends, with no crash
    dialog.
  - `HKCU\Software\Wine\DllOverrides` `winemenubuilder.exe=""` (disabled). shell32
    starts it to write host menu entries and file associations; Proton's copy
    has these patched out by Valve's Wine. The disabled load order makes the start
    fail instead, which is Wine's documented way to turn it off.
  - `HKCU\Software\Wine\DllOverrides` `control.exe=""` (disabled), for now. mshtml
    without Gecko and mscoree's registration without Mono start
    `control.exe appwiz.cpl install_gecko|install_mono` and wait for it without a
    timeout; appwiz shows a download dialog, a window the phone does not show. Proton
    never reaches that path because it ships Gecko and Mono. Until the app ships
    them (an owner decision, PLA-78's component list), `control.exe` is present but
    does not start, which is the runtime behaviour 0070 had. Remove the override
    with the Gecko/Mono decision.
- **Wine modules Proton ships that the build does not make stay out**, by reason:
  - The 28 modules WineHQ 11.18 builds only for i386 (`crtdll`, `d3d8`, `dm*`,
    `dplay*`, `msvcp70/71`, `msvcr70/71`, `msvcrt20/40/d`, `ole*32`, `twain_32`,
    `vdmdbg`, `iccvid`, `iprop`, `msscript.ocx`, `ctl3d32`, `d3dim`, `d3dim700`;
    WineHQ `e8aa35c`…`7baf867`, "Disable 32-bit only modules on 64-bit targets"). No
    64-bit Windows had them, so no 64-bit program can import them; Proton's x86_64 set
    still has them because Valve's Wine is based before that change. Their i386 builds
    are staged. Building them for arm64ec is one configure line in
    `build/stages/wine-pe.sh` if a title ever asks.
  - The i386 copies of the host-architecture programs (`conhost`, `services`, `rpcss`,
    `plugplay`, `spoolsv`, `termsv`, `wineboot`, `winemenubuilder`, `wuauserv`): Wine
    builds them only for the native architecture under new WoW64; Proton's i386 copies
    serve its old-WoW64 mode. Their aarch64 builds are staged.
  - Modules needing a unix library the app does not build (`kerberos`: krb5/GSSAPI,
    PLA-50; `wineusb.sys`: libusb) or a Linux host (`winealsa`, `winepulse`, `winex11`).
  - `wow64cpu.dll`, the x86-64 host's WoW64 CPU (ours is FEX's `xtajit.dll`).
  - Valve-only modules (`lsteamclient`, `vrclient`, `wineopenxr`, `steam.exe`,
    `protontts`, `audioses`, `amd_ags_x64`, `amdxc64`, `atiadlxx`, `nvcuda`,
    `light.msstyles`, `belauncher`, `dotnetfx35`, `getminidump`, `tabtip`,
    `sharedgpures.sys`) are Proton components, not Wine's: each needs Valve's patches
    or a dependency, and is listed for the owner with the other Proton components in
    the [evidence](../evidence/2026-10-08-proton-dll-parity.md), not started.

## What it costs

- **Size.** +574 files and +347.0 MB (330.9 MiB) in the bundle over 0070 (aarch64
  +512 files, +338.2 MB; i386 +62, +8.8 MB). The stored dev IPA goes from
  1,181.4 MB to 1,528.9 MB, the release IPA from 1,175.0 MB to 1,522.5 MB. Deflated,
  the added files are 40.4 MiB.
- **Registry.** Two marked user keys; no other key changes (the COM seed still comes
  from the arm64ec set only).
- **Behaviour.** A crash goes through winedbg (a backtrace in the log) instead of
  failing to start a debugger; the title still ends. The aarch64 modules are files
  until a native process loads them.

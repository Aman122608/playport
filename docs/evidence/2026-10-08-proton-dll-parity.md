# 2026-10-08: Proton parity for the staged Wine modules (PLA-78)

**Change:** commits `77e125e` ([decision 0070](../decisions/0070-wine-modules-proton-parity.md):
arm64ec and i386 whole, aarch64 a companion set) and `2c4b50d`
([decision 0071](../decisions/0071-wine-modules-full-proton-set.md): every Wine module
Proton ships, aarch64 and 16-bit included) on `dev/pla-78-proton-dll-parity`.
**Reference:** the local Proton 10.0 dist (`proton-10.0-4b`), with Proton Experimental
(`experimental-11.0-20261001`) where it differs. **Phone:** iPhone18,4, iOS 27.0, dev
app, each game's saved backend; unattended.

## IPAs

| IPA | sha256 | bytes | files |
| --- | --- | ---: | ---: |
| dev, before (`08836ab`) | `cdbeaf3322fe4bf0f79f55f7b2fb514edbd6bb7594739082467473e8a64a5edb` | 678,882,758 | 1,623 |
| dev, 0070 (`77e125e`) | `4a738948581b55cf7aed68d31902de7b0bbe34abc2eb5f39ec588a9c440245c1` | 1,181,431,404 | 2,379 |
| dev, 0071 (`2c4b50d`) | `44b6e3dcc2df40238b3af47eaa189a6c30eacddc0b8400003d0caf92a02110e7` | 1,528,858,556 | 2,953 |
| release, 0070 (`77e125e`) | `dd7ac082c834d9b0c691e0a45afa338176ac3e5bd0bd9d4dd5035e6feca3262a` | 1,175,026,597 | |
| release, 0071 (`2c4b50d`) | `f4303489c8ebcbc13de2d4a82f1e1bfe12130f58f6fdaef57ab07822b372c9b6` | 1,522,454,901 | |

The IPA is stored, not deflated. 0071 adds 574 files and 347,009,024 bytes
(330.9 MiB) of runtime; the dev IPA grows by 347,427,152 bytes, the release IPA by
347,428,304. Deflate-6 of the added files is 40.4 MiB. Together with 0070 the
runtime grew by 1,330 files and 810 MiB. All four IPAs pass `pp verify` (80 dev, 84
release checks); `pp test` passes.

## Staged sets

| Set | Before 0070 | 0070 | 0071 | Rule (0071) |
| --- | ---: | ---: | ---: | --- |
| `arm64ec-windows` | 149 | 650 | 650 | all 647 production modules, FEX `xtajit64`, `P2-ec` ntdll, DXMT `winemetal`/`d3d11`/`dxgi`/`d3d10core`, session root, URL opener |
| `aarch64-windows` | 128 | 239 | 751 | all 749 production modules (DXMT `d3d11`/`dxgi` over Wine's), DXMT `winemetal`, FEX `xtajit` |
| `i386-windows` | 624 | 767 | 829 | all 829 production modules, 62 16-bit modules included |

Production = the manifest's `dlls/` and `programs/` entries, no `tests/`, no
`winetest.exe`. `winedbg.exe`, `control.exe` (aarch64, i386) and
`winemenubuilder.exe` (aarch64 only: Wine builds it for the native architecture)
are now staged.

## Inventory against Proton's `lib/wine`

Proton's `x86_64-windows` (765 files, 291.4 MiB) against our 64-bit side, and its
`i386-windows` (822, 273.2 MiB) against our i386 set. Sizes are Proton's on disk.

| Class | x86_64 vs arm64ec ∪ aarch64 | x86_64 vs arm64ec alone | i386 vs i386 |
| --- | --- | --- | --- |
| In Proton and staged (0070 → 0071) | 712 → **715** (260.1 MiB) | 615 → 615 (239.1 MiB) | 732 → **794** (255.0 MiB) |
| In Proton, built, not staged | 3 → **0** (`control`, `winedbg`, `winemenubuilder`) | 0 | 62 → **0** (60 16-bit, `control`, `winedbg`) |
| In Proton, not built: WineHQ 11.18 builds it only for i386 | 28 (12.4 MiB) | 28 | – |
| In Proton, not built: host-architecture program (our aarch64 copy is staged) | – | 100: 97 programs, `chcp.com`, `wow64`, `wow64win` (all staged in aarch64) | 9 (1.7 MiB): `conhost`, `plugplay`, `rpcss`, `services`, `spoolsv`, `termsv`, `wineboot`, `winemenubuilder`, `wuauserv` |
| In Proton, not built: unix library or Linux host | 5 (0.4 MiB): `kerberos`, `wineusb.sys`, `winealsa`, `winepulse`, `winex11` | same | same |
| In Proton, not built: x86-64 host WoW64 CPU | `wow64cpu.dll` (ours: FEX `xtajit.dll`) | same | – |
| Proton-only (Valve's Wine) | 16 (18.5 MiB) | same | 14 (16.1 MiB) |
| Ours, not in Proton | 41 (newer Wine modules such as `windows.ui.xaml`, `winsqlite3`, `icuuc`; `winemac.drv`; FEX, DXMT, Playport's two programs) | | 35 |

The 28 i386-only modules: `crtdll`, `ctl3d32`, `d3d8`, `d3dim`, `d3dim700`, `dmband`,
`dmcompos`, `dmime`, `dmscript`, `dmstyle`, `dplay`, `dplayx`, `dpwsockx`, `iccvid`,
`iprop`, `msscript.ocx`, `msvcp70`, `msvcp71`, `msvcr70`, `msvcr71`, `msvcrt20`,
`msvcrt40`, `msvcrtd`, `olecli32`, `olepro32`, `olethk32`, `twain_32`, `vdmdbg`. WineHQ
disabled them on 64-bit targets in 11.18 (`e8aa35c`…`7baf867`); Proton's Wine predates
that, so Proton 10.0 and Experimental still ship x86-64 builds (Experimental: 27, no
`twain_32`). Their i386 builds are staged. Not built for arm64ec (decision 0071): no
64-bit Windows had them; one configure line in `build/stages/wine-pe.sh` would add
them.

Proton Experimental's `x86_64-windows` (781) and `i386-windows` (840) give the same
classes: 732 and 811 staged, nothing built and left out, plus `igd10iumd64.dll`
(Valve's Intel shim) among the Proton-only modules.

Wine's own fonts (Proton's `share/wine/fonts`, 0.8 MiB) are not modules and are not
in this change: the app ships `tahoma`/`tahomabd`; `marlett`, `symbol`, `webdings`
and `wingding` are tracked prebuilts like those (46 KB in all); Proton's 50 `.fon`
bitmap fonts need `sfnt2fon` with FreeType in the build tools, which the
`build-tools` tree is configured without.

## Proton's own components (not added; for the owner)

Sizes are Proton 10.0's on disk; "stage" means a locked input and a staging step,
"build" a new build stage.

| Component | Size | Licence | Needs | Recommendation |
| --- | ---: | --- | --- | --- |
| wine-mono 10.4.1 (`share/wine/mono`) | 225.2 MiB | MIT (Mono, Wine Mono) with its bundled components' own licences | pin + stage (our Wine 11.19 expects Mono 11.3.0); registry from its MSI | **Yes, first.** .NET Framework/XNA titles (no own runtime) fail without it; Unity titles do not need it |
| wine-gecko 2.47.4 x86 + x86_64 (`share/wine/gecko`) | 104.2 + 103.0 MiB | MPL-2.0 | pin + stage; no ARM64 build exists, the x86-64 one would run under FEX in an ARM64EC process | Later, with the first title whose launcher or UI needs HTML; it also lets the seed's `control.exe` override go |
| Proton fonts (`share/fonts`: Arial, Courier New, Georgia, Times, Microsoft Sans Serif, MS Gothic, Microsoft YaHei, SimSun, Malgun, Nirmala substitutes) | 70.2 MiB | SIL OFL 1.1 (Liberation, Noto/Source Han derived) | pin + stage (font sources), font registry | **Yes, second.** Text metrics and CJK glyphs; only Tahoma is shipped now |
| DXVK i386 `d3d8`/`d3d10core`/`d3d11`/`dxgi`, arm64ec/i386 `openvr_api_dxvk` (`lib/wine/dxvk`) | 27.6 MiB in Proton (ours: arm64ec set + i386 `d3d9`) | zlib | build flag in `build/stages/vulkan-pe.sh` | Yes, for 32-bit Direct3D 10/11 titles on the Vulkan backend; separate issue |
| vkd3d-proton i386 (`lib/wine/vkd3d-proton`) | 10.4 MiB total | LGPL-2.1 | build flag | No until a 32-bit Direct3D 12 title |
| dxvk-nvapi (`lib/wine/nvapi`) | 4.9 MiB | MIT | pin + build | No: NVIDIA-only features (DLSS, Reflex), no NVIDIA GPU |
| ICU 68 (`lib/wine/icu`) | 63.7 MiB | Unicode License | pin + stage | No: Wine 11 builds `icuuc`/`icuin`, staged |
| `lsteamclient`, `steam.exe` | 5.5 + 4.4 MiB, 0.1 MiB | Valve's Wine (LGPL-2.1) bridging the native Steam client | Valve's patches + a Steam client on the host | No: Playport's Steam API emulator and host session do this |
| `vrclient`, `wineopenxr` | 4.1 + 3.4, 0.3 MiB | LGPL-2.1 / BSD | Valve's patches + OpenVR/OpenXR runtime | No: no VR on the phone (Hollow Knight and Portal 2 probe `wineopenxr` and get "missing", harmlessly) |
| `protontts` + espeak-ng data + tashkeel model | 0.1 + 17.2 + 9.8 MiB | GPL-3.0 (espeak-ng), MIT (Piper) | Valve's patches + Piper/espeak-ng build | No for now (speech synthesis) |
| `amd_ags_x64`, `atiadlxx`, `amdxc64`, `nvcuda` (Experimental: `igd10iumd64`) | 0.7 MiB | Valve's Wine | Valve's patches | No: GPU-vendor shims for desktop GPUs |
| `audioses`, `light.msstyles`, `belauncher`, `dotnetfx35`, `getminidump`, `tabtip`, `sharedgpures.sys` | 7.6 MiB | Valve's Wine | Valve's patches | `belauncher` (BattlEye launcher shim) and `tabtip` (on-screen keyboard) only with a title that needs them |
| xalia (`share/xalia`) | 4.2 MiB | MIT (its repository; the dist has no licence file) | pin + .NET build | No: gamepad navigation of launcher windows the phone does not show |
| `default_pfx` | 29.3 MiB | (generated) | – | No: the app seeds its prefix from `app/registry` |

## Winedbg, control and winemenubuilder

What Proton does: Valve's `wine.inf` writes `HKCU\Software\Wine\WineDbg`
`ShowCrashDialog=0`; `AeDebug` keeps `winedbg --auto %ld %ld` with `Auto=1`, so a
crash writes a backtrace and ends. Valve's Wine patches `winemenubuilder` (no menu
entries, no associations, `.desktop` files under `c:\proton_shortcuts`); the
`proton` script sets no override for it. Gecko and Mono prompts never happen
because the dist ships both.

What the app does (decision 0071, `NEUTRALISE` in `app/tools/prefix-registry.py`,
marked for top-up): `ShowCrashDialog=0` as Proton; `winemenubuilder.exe` and
`control.exe` disabled through `HKCU\Software\Wine\DllOverrides`, the latter until
Gecko and Mono are decided. On the phone's existing prefix the seed added all three
values on the first launch (`registry user.reg: 0 of 6 seed keys added, 3 values
added to 2 present keys`), and the next launches found them.

Portal 2 crash path: 0070's one crash (`20261008T175727`, on `d5e26d64`) went through
`winedbg --auto`, whose "Program Error" dialog failed to be created; the title ended
1.8 s after its first frame. With `ShowCrashDialog=0`, winedbg skips the dialog
(`display_crash_dialog` returns at once) and prints the backtrace. Five Portal 2
plays on `44b6e3dc` reached first frame +10 with no crash, so the path was **not
exercised on the phone** in this change.

## Plays

Settings as saved on each game's page; `--until first-frame+10` (Valheim +20).

| Run | IPA | Title | First frame | JIT | Pool head / tail live / room MB, images | Band used / other MB |
| --- | --- | --- | ---: | ---: | --- | --- |
| `20261008T174231` | `cdbeaf33` | Hollow Knight (Steam 367520) | 11.48 s | 2.98 s | 81 / 145 / 271, 57 | 3,427 / 375 |
| `20261008T180701` | `4a738948` | Hollow Knight | 10.21 s | 2.54 s | 82 / 145 / 270, 57 | 3,491 / 423 |
| `20261008T185657` | `44b6e3dc` | Hollow Knight | 9.63 s | 2.34 s | 83 / 145 / 269, 59 | 3,475 / 423 |
| `20261008T174323` | `cdbeaf33` | Portal 2 (Steam 620) | 6.02 s | 2.63 s | 26 / 161 / 326, 18 | 1,835 / 183 |
| `20261008T180752` | `4a738948` | Portal 2 | 5.72 s | 2.46 s | 26 / 161 / 326, 18 | 1,757 / 236 |
| `20261008T-p2-44b6e3dc` (+ `-2`, `-3`, `-4`) | `44b6e3dc` | Portal 2 | 5.76 (5.64, 5.82, 5.85) s | 2.47 (2.33, 2.45, 2.55) s | 26 / 161 / 326, 18 (all four) | 1,755–1,911 / 135–276 |
| `20261008T180857` | `4a738948` | Valheim (Steam 892970) | 7.31 s | 2.52 s | 103 / 145 / 249, 83 | 4,845 / 532 |
| `20261008T190206` | `44b6e3dc` | Valheim | 7.32 s | 2.40 s | 103 / 145 / 249, 83 | 4,837 / 508 |

All reached their `--until` point and were ended by the driver (`exit 0`). The
Hollow Knight screenshot shows the title menu. The pool figures do not move: Hollow
Knight's log names the same 60 pool copies as on `4a738948` (the result's image count,
57 and 59, is sampled at the stop); Portal 2 and Valheim match to the megabyte.
Run `20261008T185804-mixed-p2-valheim` holds two runs started together and is not
used.

**Failed DLL loads** (`s1-host.log`, `dll-missing`, `c0000135`, `NOT FOUND`,
`aarch64-windows`), before (`4a738948`) and after (`44b6e3dc`): the same set in each
pair. Hollow Knight: 60 `c0000135` lines, the game's own probes (`UnityEngine.*.dll.dll`
under its folder) and `AMDUnityPlugin`, `NVUnityPlugin`, `openvr_api_dxvk`,
`wineopenxr`. Portal 2: 23, its own `*shader_dx*.dll` probes and
`openvr_api_dxvk`, `wineopenxr`; three `[ec-child-ntdll] … NOT FOUND` export probes,
unchanged. Valheim: 60, the game's own probes, `AMDUnityPlugin`, `NVUnityPlugin`. No
Wine module is missing in any run. `aarch64-windows` appears only as the sysarm64 link
farm (751 links after, 239 before) and, for Portal 2, the WoW64 child's native
`ntdll.dll`. No `winedbg`, `winemenubuilder` or `control.exe` line in any run.

Screenshots (`screen-stop.png`) stay in `.work`.

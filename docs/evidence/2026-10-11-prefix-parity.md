# 2026-10-11: prefix parity with a fresh wineboot prefix (PLA-163)

**Linear:** [PLA-163](https://linear.app/playportdev/issue/PLA-163). Decision
[0080](../decisions/0080-prefix-wineboot-parity.md); the Steam redistributables decision is
[0081](../decisions/0081-steam-common-redistributables.md).

## What was compared

- A fresh `wineboot --init` prefix of the workstation's Wine 11.18 (win64). The pin is wine-11.19;
  between the two, `loader/wine.inf.in` is unchanged, `kernelbase.rgs` gains one value (in the seed
  already: it is generated from the 11.19 DLLs) and no media DLL's registration code changes, so for
  registrations the two are the same.
- Fresh Proton 10.0-4b and Proton Experimental (11.0) prefixes.
- The phone's prefix (`system.reg`, `user.reg`, `userdef.reg`, a listing of `drive_c/windows` over
  AFC) on the build before this change, and the seed before and after.

The per-key table and the classification are in the build area
(`$PLAYPORT_BUILD/agent-notes/pla163/prefix-diff.md`, with the comparison script).

## Registry: what the phone lacked

Against the Wine reference, keys only in the reference, by category (values missing on shared keys
in brackets): 32-bit class view 6,860 and `Software\Wow6432Node` 802 (both by decision 0072);
COM interfaces 1,132 and typelibs 56 (typelib registration, mostly mshtml's); other classes 299
(file types, ProgIDs of i386-only modules, DMOs); `Windows\CurrentVersion` 134 [24];
`Windows NT\CurrentVersion` 20, including the key's own version values; COM classes 49 (DirectShow
filter instances and categories, shell folder attributes, iexplore); Media Foundation 14; crypto 17
(cryptnet, cryptdlg; 0078 leaves them out); DirectX 8; fonts 3 [2,457, nearly all host fonts];
services and devices (host data) the rest.

## What the seed now carries (additive; marked for top-up)

| Source | Sections added |
|---|---|
| wine.inf AddReg of `DefaultInstall.ntamd64` (18 sections), interpreted | system and user keys for Windows version, program-files paths, DirectX, DirectPlay, DirectMusic, MCI, font substitutes, file associations, Session Manager, LSA, Rpc, ThemeManager, UrlAssociations, AeDebug |
| urlmon REGINST | security zones and zone map, HKLM and HKCU |
| media registrations (`wineboot-media.reg`, 59 sections, 14 DLLs) | 18 DirectShow filters, 9 filter categories, 8 DMOs, 7 MFTs and their category keys, l3codecx's class |
| i386-only modules' scripts (8 modules) | DirectMusic, DirectPlay, msscript, olepro32 classes and ProgIDs |

System sections 7,382 → 8,072, user sections 6 → 29. No key or value of the previous seed changed or
went. Every value generated from wine.inf equals the reference prefix's, except
`TimeZoneInformation`, which the runtime sets from the host. The seed still differs from the reference
by design in: the 32-bit view, services, host and device data, typelib registrations, shell folder
attributes and known-folder descriptions (lookups fall back to the built-in table), mshtml's
registration (no Gecko), REG_LINK keys.

## Files

494 of the reference's 1,871 `drive_c/windows` entries are not in the phone's prefix: 81 `.nls`
files (ntdll reads the runtime's `nls` directory), Wine's fake device/service folders (`drivers`,
`spool`, `driverstore`, `tasks`, `wbem`), 34 `winsxs` entries (the runtime links its own winsxs
assemblies) and about 100 64-bit programs (`reg`, `start`, `taskkill`, `xcopy`, …): the arm64ec build
has 8 programs and the aarch64 programs are linked into `sysarm64`, so an x86-64 caller, or `cmd`'s
`start`, finds none in `system32`. That last one is a follow-up, not changed here.

## Library import census (host only, aggregate)

23 installed titles on the workstation were walked (static and delay imports of every game PE,
redistributable folders skipped). 12 have an import neither the title nor Playport provides; by likely
source: 19 title-specific or optional (Miles, Qt plugins of a launcher, engine plugin DLLs), 5 optional
vendor GPU SDKs, 2 NVIDIA/PhysX, 2 VC++ redistributable (`mfc140u` from a Razer Chroma plugin, `mfc42`
of an installer skin), 1 OpenAL. 2 titles are i386 D3D9, one launcher imports i386 D3D11, none
carries a .NET assembly, 6 ship a Steam `installscript.vdf` naming `_CommonRedist` installers
(VC++ 2008/2010, DirectX June 2010, OpenAL), the case decision 0081 covers. Every Unity and Unreal
title imports `opengl32` (player and crash reporter) without rendering with it. The per-title census
stays in the build area.

## Build and phone

First build: IPA sha256 `1795529972f85a6c7808d7d5209c53f774ba81a05610f437635523e759068d8a` (dev, `pp verify`
80/80), not installed: `pp install` failed twice with installd's `PackageExtractionFailed` while the phone
had 2.1 GB free. After uninstalling three finished titles through the UI (15.1 GB free) the same commit
built as `30b4191763e6024c221e703223e5b93502473b32b70171a9718bc751ebe75ce8` installed: Hollow Knight
reached its first frame (+9.92 s), but Portal 2 (i386) ended at once with `0xc0000135`
(`STATUS_DLL_NOT_FOUND`). The seed had brought `[CurrentVersionWow64.ntamd64]`'s
`HKLM\Software\Microsoft\Wow64\x86` = `wow64cpu.dll`, which wow64 reads to pick the i386 emulator
DLL (`dlls/wow64/syscall.c` `get_cpu_dll_name`); the app ships FEX's `xtajit.dll`, wow64's default on
an ARM64 host. The generator now skips `Software\Microsoft\Wow64` (test
`test_no_wow64_emulator_key_names_an_x86_64_hosts_cpu_dll_for_i386_games`), and the key the top-up had
already written to the phone's prefix was removed from its `system.reg` over AFC (top-up only adds, so a
prefix that ran the first build keeps it; only this phone did).

Fixed build, IPA sha256 `ee48c99290b36f99e425392d33ec5828af705d41d762fce99e67c0f8df345953`: Portal 2 first
frame +5.87 s (Source splash), House of Golf 2 first frame +7.27 s (its prerequisite skipped as already
run; black after the splash, as before this change), Hollow Knight first frame +9.03 s (main menu). The
phone's `system.reg` pulled after these runs has the top-up: `ProgramFilesDir`, `CurrentBuildNumber`
19045, 16 `DirectShow\MediaObjects` sections, and no `Wow64\x86` key. Host checks:
`tools/tests/test_prefix_registry.py` (wine.inf semantics, digest guards, the required registrations),
`pp registry --check` through the build's stage step.

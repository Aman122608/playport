# 0080: The prefix seed is what wineboot writes, read from Wine's own sources

**Status:** accepted, 2026-10-11, by the PLA-163 worker on the owner's direction of that day ("stop
finding prefix gaps one game at a time; make the prefix what Wine/Proton's first boot makes, all at
once"), with the owner away (the conservative option at each choice, recorded for review);
[PLA-163](https://linear.app/playportdev/issue/PLA-163). Adds to 0070–0072, 0077 and 0078's seed;
amends 0072's "i386-only registrations" gap.
[Evidence](../evidence/2026-10-11-prefix-parity.md).

## Context

The app never runs wineboot, so its prefix gets only what `app/tools/prefix-registry.py` seeds. That
was each builtin's `WINE_REGISTRY` scripts plus registrations added one title at a time (WIC,
wintrust, msisip, the VC runtime key, Wine Mono's keys, the network database path). A comparison with
a fresh `wineboot --init` prefix of the same Wine found the rest of wineboot's work missing as a
whole: wine.inf's registry (no `Windows NT\CurrentVersion` version values, no `ProgramFilesDir`, so
no `%ProgramFiles%` in any process, no DirectX version key, no file associations, no font
substitutes), urlmon's security zones, the DirectShow filters, DMOs and most Media Foundation
transforms the media DLLs register at run time, and the classes of the modules Wine builds only for
i386 (DirectMusic's performance and segments, DirectPlay's lobby).

## Decision

- **wine.inf's AddReg, interpreted.** The seed applies every AddReg section `[DefaultInstall.ntamd64]`
  and the `[BaseInstall]` it needs list, read from the build's generated `wine.inf`, with setupapi's
  `do_reg_operation` semantics (types, `NOCLOBBER`, `KEYONLY`, string and directory-id substitution);
  the port is guarded by that function's digest. A Wine move that adds a line brings it in. Left out:
  REG_LINK values (a seed section cannot carry wineserver's link flag), `[PreInstall.ntamd64]`'s Wow64
  view (0072) and the services (`AddService`; the app runs none).
- **REGINST resources.** urlmon's (zones, zone map) are applied from the staged DLL the same way.
  mshtml's are left out: its classes need Gecko, which the app does not ship.
- **Media run-time registrations as data.** DirectShow filters with their FilterData, the filter
  categories, DMOs and the MFTs the hand-ported `MFTS` table lacks are written by
  `IFilterMapper2`/`DMORegister`/`MFTRegister` calls; porting each table is large. They are kept in
  `app/tools/wineboot-media.reg` as a desktop prefix of the same Wine has them, each section naming
  its DLL, and each DLL's `DllRegisterServer` is digest-guarded: a Wine change to one stops the
  generation until the file is refreshed from a new desktop prefix.
- **i386-only modules' classes in the 64-bit view.** Their scripts are applied into the same
  `Software\Classes`, where i386 callers read them through the shared-key fallback; a 64-bit module's
  key or value wins. This closes 0072's known gap without creating `Wow6432Node`.
- **Additive and marked for top-up**, as the rest of the seed: no key or value of the previous seed
  changes, and an existing prefix gets each value it lacks at the next launch; values it holds stay.
- **A committed list of required registrations** (`tools/tests/test_prefix_registry.py`
  `REQUIRED`) fails `pp test` if a pin move loses one.

Not done here, each a follow-up issue: typelib registration of the shipped `.tlb` files and TYPELIB
resources (stdole2, mshtml, activeds); the run-time values wineboot computes from the machine
(`NUMBER_OF_PROCESSORS`, `PROCESSOR_*`, computer name); the 64-bit programs (`reg`, `start`,
`taskkill`…), whose aarch64 builds are linked into `sysarm64`, not `system32`; and
`CurrentControlSet` as a link to `ControlSet001`.

## Why

The owner's direction: parity with Wine's first boot in one step rather than per title. Reading
Wine's own `wine.inf` and DLL resources keeps the seed in step with the pin without a second copy of
their content; only the media registrations, which no file in the tree holds, are data, guarded the
way 0072 and 0078 guard their ports. Running wineboot on the phone was not an option (one process, no
services, decision 0029), and shipping a desktop prefix's whole hive would carry host data.

## Consequences

- The seed grows from 7,382 to 8,072 system sections and from 6 to 29 user sections; system.reg from
  1,180,858 to 1,314,664 bytes and user.reg from 4,532 to 12,118.
- Processes now see the registry's Windows version (10.0.19045, Windows 10 Pro, as desktop Wine), and
  `%ProgramFiles%`, `%CommonProgramFiles%` and their x86 forms.
- `AeDebug` names `winedbg --auto`, as kernelbase's default did; `ShowCrashDialog=0` (0071) still
  keeps the dialog off.

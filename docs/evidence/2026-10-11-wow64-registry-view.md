# Evidence: the 32-bit registry view for i386 callers (audit, no change)

**Date:** 2026-10-11. **Linear:** [PLA-175](https://linear.app/playportdev/issue/PLA-175)
(i386 callers read `HKLM\Software` and HKCR without a 32-bit view), follows
[PLA-170](https://linear.app/playportdev/issue/PLA-170) and
[PLA-163](https://linear.app/playportdev/issue/PLA-163). **Decisions:** confirms
[0072](../decisions/0072-seed-wic-registration-64-bit-classes.md),
[0080](../decisions/0080-prefix-wineboot-parity.md) and
[0085](../decisions/0085-dotnet-framework-directories.md) (whose "follow-up audit" this is); no
new decision, no runtime change, no phone run. Scratch, scripts and full tables:
`$PLAYPORT_BUILD/agent-notes/pla175/` (`wow64-diff.md`, `wowdiff.py`, `wowresolve.py`,
`wowprobe.c`, `probe-*.txt`).

## How an i386 caller reads the registry

Read in the Wine tree at the pin (`server/registry.c`, `server/mapping.c`,
`dlls/kernelbase/registry.c`); Playport's series change none of it (madeira-unix's only
`server/registry.c` change makes `RegFlushKey` write to disk).

- wineserver treats a process as WoW64 when the machine is 64-bit and the process's machine,
  which is its main image's (`mapping.c`), is 32-bit. An i386 game's pseudo-process is one.
- For a WoW64 process every open or create without `KEY_WOW64_64KEY` follows, at each path
  element, that key's `Wow6432Node` child **if the child exists**, with no fallback. The one
  exception is `Software\Classes\Wow6432Node`, which the server creates at start as shared
  (`KEY_WOWSHARE`): a first-level class name it lacks is looked up in `Software\Classes`.
- kernelbase leaves a WoW64 process's open to the server. Its client-side emulation is for a
  64-bit caller passing `KEY_WOW64_32KEY`, and it does not create a missing `Wow6432Node`.

So redirection is on, as on desktop Wine. The seed has no `Software\Wow6432Node`, so an i386
caller reads the one 64-bit `Software` view, and its classes through the shared fallback. A
seeded `Software\Wow6432Node` would be read automatically, and would hide every other key.

## What differs between the views in a wineboot'ed prefix

Compared within each reference prefix of PLA-163 (desktop `wineboot --init` of Wine 11.18, and
Proton 10.0-4b and Experimental 11.0), with the 50 `Wow6432Node` keys that wine.inf's `[Wow64]`
section makes REG_LINKs to the 64-bit key followed (Fonts, FontSubstitutes, OLE, Policies, Image
File Execution Options, Cryptography Services, SystemCertificates, `Classes\Wow6432Node\TypeLib`,
`AppId`, `Protocols`, …):

| Key | 32-bit view | 64-bit view (what i386 reads here) | Readers | Outcome |
|---|---|---|---|---|
| `Windows\CurrentVersion` `ProgramFilesDir`, `CommonFilesDir` | `C:\Program Files (x86)…` | `C:\Program Files…` | ntdll's environment, shell32's CSIDLs and msi already give a WoW64 process the x86 paths (they read the `(x86)` values or choose in code); only a direct registry read (NSIS 2's `$PROGRAMFILES`) gets the 64-bit path, which exists | no change |
| `MediaPlayer` `Installation DirectoryLFN` | `…(x86)\Windows Media Player` | `C:\Program Files\…` | none known | no change |
| `.NETFramework` `InstallRoot` (Mono) | `…\Framework\` | `…\Framework64\` | mscoree | fixed by wine-pe 0036 (0085) |
| `NET Framework Setup\NDP` `Version` (Proton) | `2.2.50727.4927`, `3.0.30729.4926` | `…5420` | presence checks | no change |
| `VisualStudio\14.0\VC\Runtimes\x86` | present | absent in a fresh seed | an x86 VC++ redistributable's own check | no change: the redistributable runs and writes it into the one view (the owner's phone has it) |
| `Classes`: ADO, OLE DB, Ink `InprocServer32`; IE `LocalServer32`/icons (25 values) | `Program Files (x86)\Common Files\…` | `Program Files\…` on desktop; the seed names `C:\windows\system32\<dll>`, which maps to `syswow64` for i386 | COM | already right for i386 |
| `Classes`: 182 CLSID and 28 Interface keys of 44 classes Wine builds only for i386 (dmime, dmscript, dmstyle, dmcompos, dmband, dplayx, msscript) | 32-bit only | in the seed's 64-bit classes since 0080 | DirectMusic, DirectPlay, ScriptControl | covered |
| `Classes`: `ThreadingModel`, CLSIDs only in the 64-bit view | identical; none (the 64-bit-only keys are first-level extensions, ProgIDs and MIME types, shared on desktop too) | | | none |
| TypeLib `win32` vs `win64` | desktop keys carry both | the seed has `win64` | oleaut32 tries the other syskind | none |

Everything else in the 32-bit view (DirectX `Version`, DirectPlay providers, DirectMusic,
`Windows NT\CurrentVersion`, Rpc, Internet Settings) is `[BaseInstall]` written into both views
with the same values.

## Desktop A/B: what an i386 program reads

An i686 probe (`wowprobe.c`, llvm-mingw) reads values and creates COM objects under the
workstation's Wine 11.18 (Arch `wine 11.18-1.1`), in a scratch prefix whose `system.reg` is in turn
the wineboot'ed one, the committed seed (`app/registry/system.reg` at 715616e) and the seed after a
64-bit `reg add HKLM\Software\Wow6432Node\OtherVendor\Game`:

| i386 read | wineboot | seed | seed + one `Software\Wow6432Node` key |
|---|---|---|---|
| `ProgramFilesDir` (RegQueryValue) | `C:\Program Files (x86)` | `C:\Program Files` | not found |
| `%ProgramFiles%` | `C:\Program Files (x86)` | `C:\Program Files (x86)` | `C:\Program Files (x86)` |
| `.NETFramework InstallRoot` | (no Mono) | `…\Framework64\` (mscoree maps it, 0036) | not found |
| DirectX `Version`, `ProductName` | `4.09.00.0904`, `Windows 10 Pro` | same | not found |
| VC `Runtimes\x86` | `14.42.34433.0` | not found | not found |
| CoCreateInstance DirectMusicPerformance, FilterGraph, DirectPlayLobby, XAudio2 2.7 | `S_OK` ×4 | `S_OK` ×4 | `REGDB_E_CLASSNOTREG` ×4 |

The third column is the one hazard the audit found: once any `Software\Wow6432Node` key exists, an
i386 caller loses all of `HKLM\Software`, and with it HKCR (`Software\Classes` is looked up under
`Wow6432Node`, where Wine then created a plain `Classes` key), for the prefix's lifetime. A program
that writes an explicit `Wow6432Node` path, as some launchers and installers do, would cause it.
`reg add … /reg:32` from a 64-bit process did not (the key went to the 64-bit view). The owner's
phone hive (2026-10-11, before PLA-163's seed) has no such key. It is left for a follow-up issue:
handling it changes how the registry is read, not the seed.

## Outcome

Option (c) of the three the issue named: no further registry change. (a), seeding
`Software\Wow6432Node` and `Classes\Wow6432Node` from the i386 registrations, would need the whole
seed mirrored plus the 50 links, which the seed cannot carry, and would hide from i386 callers every
key i386 installers and games have already written into the one view of existing prefixes
(the owner's has the VC++ x86 runtime key, for one). (b), more per-reader fixes, has no reader to
fix: the only value differences with a known reader are `InstallRoot` (done, 0036) and the Program
Files paths, which the environment, shell32 and msi already give right.

`tools/tests/test_prefix_registry.py` `REQUIRED_I386` now guards the 32-bit view as the seed models
it: the i386-only modules' classes and the classes i386 games create are in the 64-bit classes with
their `ThreadingModel`, the 64-bit `InstallRoot` 0036 maps is there, and so are the x86 Program Files
values ntdll reads for a WoW64 process; no `Software\Wow6432Node` appears in either hive.

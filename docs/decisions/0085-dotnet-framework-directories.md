# 0085: Wine Mono's .NET Framework directories in the prefix, and the 32-bit InstallRoot for WoW64

**Status:** accepted, 2026-10-11, by the compatibility speedrun's supervisor (option A of three,
plus the directories), with the owner away;
[PLA-170](https://linear.app/playportdev/issue/PLA-170),
[PLA-154](https://linear.app/playportdev/issue/PLA-154). Amends
[0077](0077-ship-wine-mono.md)'s "Left out: … the fake `C:\windows\Microsoft.NET\Framework*` files
its `dotnetfakedlls.inf` writes" and its "No fake Framework directories" limit.
[Evidence](../evidence/2026-10-11-i386-dotnet.md).

## Context

`mscoree`'s `LoadLibraryShim` (and `ICLRRuntimeInfo::GetRuntimeDirectory`) builds a path from
`HKLM\Software\Microsoft\.NETFramework` `InstallRoot` and a runtime version:
`C:\windows\Microsoft.NET\Framework\v2.0.50727\fusion.dll`. Steam's DirectX June 2010 setup
loads `fusion.dll` this way to check whether Managed DirectX is in use, and ended with -9 on the
phone (`LoadLibraryShim() failed, error = 0x80070006`). Under Proton it succeeds. A wineboot'ed
prefix has two things the phone's prefix lacked:

- **The files.** Wine Mono's support package runs its `support/dotnetfakedlls.inf`, whose
  `[DefaultInstall]` (32-bit) and `[DefaultInstall.ntamd64]` sections put Wine builtins
  (`fusion.dll`, `mscorwks.dll`, `ngen.exe`, …) into `Microsoft.NET\Framework\v*` and
  `Framework64\v*`; in Proton's prefix they are links to its `i386-windows` and `x86_64-windows`
  sets. 0077 left them out.
- **The 32-bit view.** The 64-bit view's `InstallRoot` names `Framework64\`, the 32-bit view's
  (`Software\Wow6432Node`) names `Framework\`. The phone's prefix has no `Software\Wow6432Node`
  (0072, 0077, 0080), so an i386 process reads `Framework64\` and cannot load the 64-bit
  `fusion.dll` there. This missing 32-bit view is the root cause of the i386 half; the .NET key
  is one case of it.

A desktop A/B in a Proton prefix showed each is needed: without the `Framework\` links, or with
the 32-bit `InstallRoot` set to `Framework64\`, DXSETUP fails exactly as on the phone.

For the 32-bit view three ways were open: (A) `mscoree` alone gives a WoW64 process the
`Framework\` directory; (B) seed `Software\Wow6432Node`, which hides every other `HKLM\Software`
key from i386 callers unless the whole seed is mirrored, a change for every installed title;
(C) drop `InstallRoot` from the seed, so `LoadLibraryShim` loads `fusion.dll` by name, which
departs from Proton's registry and makes `GetRuntimeDirectory` fail.

## Decision

- **The directories, from the inf.** At each session start the host
  (`app/Sources/WineHost/dotnet_fakedlls.c`) reads the shipped
  `Runtime/mono/wine-mono-<version>/support/dotnetfakedlls.inf`, takes the sections
  `[DefaultInstall]` and `[DefaultInstall.ntamd64]` name in `WineFakeDlls=`, and links each
  `10,<dir>,<name>` line's `C:\windows\<dir>\<name>` to the runtime's `i386-windows` or
  `arm64ec-windows` file, as syswow64 and system32 are linked. A link of an earlier bundle path is
  replaced; a file that is not a link (a real .NET install's) stays, as setupapi keeps a file that
  is not a fake DLL; a line naming another directory id, an unsafe path or a builtin the runtime
  lacks is skipped. A Wine Mono move brings the inf's new lines with it.
- **Option A for the 32-bit view** (wine-pe 0036): in a WoW64 process, `mscoree`'s
  `get_install_root` reads an `InstallRoot` ending in `Framework64\` as the `Framework\` beside
  it, what the 32-bit view says in a prefix that has one. Nothing else reads the registry
  differently; no `Wow6432Node` key is seeded.

## Why

Proton parity for the files is direct: the same inf, the same builtins. For the registry, A is
the narrowest change that gives an i386 caller the value Proton's 32-bit view gives it, without
touching how any other key is read or seeding a view that would hide the rest of the seed.

## Consequences

- DXSETUP's managed DirectX check passes on the phone; DXSETUP then stops at its first x64 cab,
  whose `infinst.exe` needs the fixed base `0x100000000`, which iOS refuses (a separate issue).
- Other i386 readers of a key whose 32-bit value differs still get the 64-bit value. Which keys
  differ, and whether to mirror them, is a follow-up audit, with this case as its evidence.
- The prefix gains `C:\windows\Microsoft.NET\Framework*\v*` with 32 links; the rest of Proton's
  Framework directories (`mscorlib.dll`, `csc.exe`, `CONFIG`, written by the support package's
  other steps) are still absent.

# Evidence: an executable fixed at 0x100000000 cannot load on iOS (DirectX's x64 infinst.exe)

**Date:** 2026-10-11. **Linear:** [PLA-174](https://linear.app/playportdev/issue/PLA-174)
(x64 relocation-stripped executable at `0x100000000`), [PLA-170](https://linear.app/playportdev/issue/PLA-170)
(Steam's DirectX June 2010 redistributable). No code change: this records an iOS limit and the
choice to leave it (supervisor, 2026-10-11). The phone run analysed is `cc-play` from the
[i386 .NET record](2026-10-11-i386-dotnet.md) (dev IPA
`747b89910fc636164c69ef59b8e0280cc886da19906d715c94efa3c59077cbc5`, iPhone18,4, iOS 27.0), under
`$PLAYPORT_BUILD/agent-notes/pla154/`.

## What fails

DXSETUP (June 2010) installs every cab in its folder, oldest first, and runs each cab's
`infinst.exe` to install its DLLs. On 64-bit Windows the x64 cab's `infinst.exe` runs. The first
one, `Feb2005_d3dx9_24_x64.cab`'s, is x86-64 with characteristics `0x23` (relocations stripped,
no base relocation directory), ImageBase `0x100000000`, SizeOfImage `0x10000`. Its `.data` holds
about 40 absolute pointers into the image, so it can run only at that base. On the phone:

    [exe-base] 0x100000000+0x10000 refused with nothing mapped there (status 0xc0000017)
    wine: failed to create main module for L"C:\\windows\\temp\\DX9A50.tmp\\infinst.exe", status c0000018

DirectX.log: `InstallEXE(): infinst.exe returned -1073741800`, then the installation ends with
that value, so Steam's HasRunKey (`Software\Valve\Steam\Apps\CommonRedist\DirectX\Jun2010`,
`dxsetup`) is not written. The game starts anyway (decision 0079). A new build then ran DXSETUP
once more (decision 0083); since [decision 0086](../decisions/0086-directx-setup-skipped.md) it is
skipped, not run ([evidence](2026-10-11-dxsetup-skipped.md)).

All 75 x64 cabs of the redistributable (extracted on the workstation) carry an `infinst.exe`
at ImageBase `0x100000000`. The 34 from Feb2005 to Nov2007 have their relocations stripped
(`0x23`). The 41 from Mar2008 to Jun2010 have a base relocation directory (`0x22`), so Wine can
place them elsewhere. Any Jun2010 DXSETUP reaches a stripped one first.

## Why: page zero is extended by the slide

The app's executable has the default 64-bit `__PAGEZERO` (`vmaddr 0`, `vmsize 0x100000000`),
with `__TEXT` at `0x100000000`, and is PIE (checked with `llvm-objdump --macho --private-headers`
on a built `S1Probe`). For a PIE executable, XNU's Mach-O loader raises the map's minimum
address to the end of page zero **plus the slide**: "for PIE, extend page zero rather than
moving it", so that early allocations cannot fall between page zero and the first slid segment.

`[0x100000000, 0x100000000 + slide)` is therefore below the task's lowest address. It shows as
free, but no fixed map there can succeed (`ENOMEM`), and the executable's own `__TEXT` starts
right at its end. In `cc-play` the slide is `0x2fdc000`:

    ml989: running image …/S1Probe.app/S1Probe base=0x102fdc000 …
    [va-scan] … firstfail=0x100010000 errno=12(Cannot allocate memory) FREE hole, next region 0x102fdc000 (+0x2fcc000 away) <-- iOS REFUSED A FREE ADDRESS

The build area's logs hold 518 distinct values of that first region above 4 GiB (the
executable's base). They range from `0x100038000` to `0x104ff4000` and are never below
`0x100010000`, so `[0x100000000, 0x100010000)` was off limits in every one of those launches. A hold like the executable window
at `0x140000000` cannot help, because nothing in the process can ever map the range. Madeira
reached the same conclusion: its address space starts at `0x100010000` ("above iOS 4GB
__PAGEZERO", madeira-unix 0023).

## What was decided

These options were weighed (supervisor, 2026-10-11):

- **Not done: a smaller `__PAGEZERO`.** Patching the linked executable's `__PAGEZERO` vmsize
  down (for example to `0xF0000000`, with `__TEXT` kept at `0x100000000`) would leave a mappable
  hole under the executable. It changes the app's memory layout:
  - Host allocations could land below 4 GiB, which the JIT pool rules, the self-check and
    madeira-unix 0084 assume never happens.
  - The kernel or dyld might refuse a 64-bit arm64 page zero smaller than 4 GiB.
  - Left for an owner decision (follow-up issue).
- **Not done: reconstructing relocations** for a stripped image by scanning for values that
  point into it. It is a heuristic, and not Proton's behaviour.
- **Not done: seeding the DirectX HasRunKey.** Proton's prefix seeds the Steam CommonRedist
  keys for .NET and XNA 4.0 (its `wine.inf`, which `app/registry/system.reg` already mirrors).
  It seeds none for DirectX:
  - Proton 10.0, 11.0 and Experimental: `wine.inf` and `default_pfx/system.reg` have no
    `CommonRedist\DirectX`.
  - In a real Proton prefix, `Software\Wow6432Node\Valve\Steam\Apps\CommonRedist\DirectX\Jun2010`
    `dxsetup` appears only after Steam ran DXSETUP there successfully.
  - Seeding it would not be parity.
- **Later (owner, decision 0086):** DXSETUP is skipped and its HasRunKey written as after a
  success. That is the seeding weighed above, chosen on Proton's builtin copy rather than on
  Proton's prefix.
- **Kept:** the limit stays. DXSETUP fails at its first stripped x64 `infinst.exe`, and the game
  still starts. Playport's builtin DLLs serve the game whatever DXSETUP copied before it failed
  (`link_system32` re-links them; the i386 .NET record shows it for `d3dx9_24.dll`).

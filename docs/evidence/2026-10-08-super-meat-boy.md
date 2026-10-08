# Evidence: Super Meat Boy (Epic), first test on the phone

**Date:** 2026-10-08. **Phone:** iPhone18,4, iOS 27.0, Wi-Fi, dev build. Driven with `pp ui`;
the session was unattended. Title: Super Meat Boy from the owner's Epic library
(`epic-Buffalo`, Epic app name `Buffalo`). Run directories are under
`$PLAYPORT_BUILD/agent-notes/super-meat-boy/`; the screenshots stay there.

**IPA:** `Playport-26.5-44b6e3dc.ipa` (dev), sha256
`44b6e3dcc2df40238b3af47eaa189a6c30eacddc0b8400003d0caf92a02110e7`: every Wine module
Proton ships (decisions [0069](../decisions/0069-wine-modules-proton-parity.md),
[0070](../decisions/0070-wine-modules-full-proton-set.md)). Not rebuilt or reinstalled for
this test.

## Profile (from the PE headers)

`SuperMeatBoy.exe` (3.6 MB, read from the Epic depot with the DLL survey's fetch):

- **i386** (`PE32`, Machine `0x14c`), Windows GUI subsystem 6.0.
- **Direct3D 9**: imports `d3d9.dll` (`Direct3DCreate9`) and **`d3dx9_43.dll`**; input
  through `DINPUT8.dll` and `XINPUT9_1_0.dll`; audio through `WINMM.dll` (its strings name
  `fmod`). The other imports: WS2_32, CRYPT32, ADVAPI32, WLDAP32, Normaliz, USER32, ole32,
  OLEAUT32, KERNEL32, SHELL32.
- **Engine:** Team Meat's own, with libcurl 7.54.1 linked in statically (the WLDAP32,
  Normaliz and CRYPT32 imports).
- **DRM / store SDK:** no Epic Online Services, Steam or Galaxy library in its imports or
  among the depot's executables (the depot's other PE files are DirectX and VC++ redist
  installers under `Prerequisites/`).

This is decision [0047](../decisions/0047-i386-titles-on-vulkan.md)'s class: an i386
Direct3D 9 title on the Vulkan route (DXVK's `d3d9` over KosmicKrisp, under WoW64 with FEX).
The app found the same: `Direct3D evidence for epic-Buffalo: Direct3D 9`.

## Install (`--action install:epic-Buffalo`)

Through Library › Epic Games with the existing signed-in session (not refreshed or
exported): build `1.2.5.3`, 519,255,478 bytes in **9 s**, catalogued as `epic-Buffalo`,
executable `SuperMeatBoy.exe` from the manifest, arguments `-epicapp=Buffalo -epicenv=Prod
-EpicPortal -epiclocale=en`. Verify before the first play: **42/42 files OK** against
Epic's manifest in 1.2 s, nothing unlisted. Free space before: 4.9 GB.

## Plays (Vulkan, the defaults)

`pp ui --play` refuses `epic-Buffalo` (its title-ID check takes lower case only; Epic app
names can have capitals), so the runs used `--action play:epic-Buffalo`, which reaches the
same Play button.

| Run | JIT | Runtime | Game started | End |
| --- | --- | --- | --- | --- |
| `play1` (verify, then play, `--until first-frame+20 --shot`) | 2.41 s | +2.84 s | +3.07 s | `0xc0000142` after 188 ms, no frame |
| `play2` (play, same `--until`) | 2.52 s | +2.95 s | +3.17 s | `0xc0000142` after 187 ms, no frame |

Both screenshots show Playport's Home screen with "Super Meat Boy stopped unexpectedly. It
exited with code 0xC0000142." over it. Pool 26 MiB head, band 214 MB used: memory and JIT
are not involved.

## Why it fails

The game never runs a line of its own code. Its i386 process (WoW64) loads its imports from
`system32`: DXVK's `d3d9.dll` (the `vulkan/i386-windows` overlay; it imports `SETUPAPI`, not
wined3d), then Wine's `d3dx9_43.dll`, which imports Wine's `d3dcompiler_43.dll`, which
imports **`wined3d.dll`**, which imports **`opengl32.dll`**:

```
002c:err:wow:wow64_NtMapViewOfSection Playport: i386 image C:\windows\system32\d3d9.dll at 79fd0000
002c:err:wow:wow64_NtMapViewOfSection Playport: i386 image C:\windows\system32\SETUPAPI.dll at 79f50000
002c:err:wow:wow64_NtMapViewOfSection Playport: i386 image C:\windows\system32\d3dx9_43.dll at 79eb0000
002c:err:wow:wow64_NtMapViewOfSection Playport: i386 image C:\windows\system32\d3dcompiler_43.dll at 79e70000
002c:err:wow:wow64_NtMapViewOfSection Playport: i386 image C:\windows\system32\wined3d.dll at 79b80000
002c:err:wow:wow64_NtMapViewOfSection Playport: i386 image C:\windows\system32\opengl32.dll at 79a40000
[unixlib] module 0x70b2390000 (opengl32.dll) WoW64 -> 0x163dba638 (stub table)
002c:err:opengl:DllMain Failed to load unixlib, status 0xc00000bb
002c:err:module:loader_init "opengl32.dll" failed to initialize, aborting
002c:err:module:loader_init Initializing dlls for L"C:\\Games\\SuperMeatBoy\\SuperMeatBoy.exe" failed, status c0000142
```

The import chain is read from the staged DLLs in the IPA (`llvm-objdump -p`): i386
`d3dx9_43` imports `d3dcompiler_43`, `d3dxof`, ole32, gdi32, user32; `d3dcompiler_43` imports
`wined3d`; `wined3d` imports `opengl32`. Both `d3dx9_43` and `d3dcompiler_43` were in
`app/artifacts.tsv` before 0069, so this is not a regression of the Proton module set.

This is the failure the compatibility plan records as class 0
([plan](../plans/2026-10-07-compat-testing-without-owning.md), found in the PLA-29 log): an
i386 process cannot initialise `opengl32.dll` because `patches/madeira-unix` 0068 gives a
WoW64 caller the all-fail stub table, whose attach fails. The plan names wined3d's own users;
Super Meat Boy shows a new path into it: **any i386 title that imports `d3dx9_*`** reaches
opengl32 through `d3dcompiler_43` and `wined3d`, even with DXVK's `d3d9`. The plan's proposed
fix (keep the GL-absent table, whose attach succeeds, for WoW64 callers) would let this load
finish; what the game does after that is not known. Not fixed here.

Ruled out: the install (42/42 verified), Epic sign-in (the process ends in the loader,
before the game reads its arguments; no EOS library), memory and JIT (both normal),
a hidden window (no `[win-pos]` line: no window is created), DXMT (no i386 build; the title is i386).
Not done: a desktop Wine/Proton run (only the depot's PE files are on the workstation and
the phone's log answers what it stops on).

**Outcome: Broken** on this IPA: exit `0xc0000142` at load, before any frame.

# Football Manager 2022: the game's own ucrtbase.dll; the Resource Archiver needs the game

**Linear:** PLA-66 (Resource Archiver); Football Manager 2022's own issue is filed from this run.

## Football Manager 2022 Resource Archiver

The archiver (Epic, `C:\Games\FM22RA`) reached a first frame and exited `0x00000001` about a
second later, on DXMT and on Vulkan. Its own log,
`AppData\Local\Sports Interactive\Resource Archiver 2022\Logs\warnings-and-errors.txt`, says:

```text
Application: Error: Unable to find some essential data. Please ensure that the game is installed correctly. If the problem persists, please contact technical support.
```

Its `special folders.xml` takes every data folder from `../FootballManager2022/data/…`: the tool
uses Football Manager 2022's data and needs the game installed beside it, as on Windows. Epic's
folder name for Football Manager 2022 is `FootballManager2022`, so Playport installs it at exactly
that path. With the game installed (6.37 GB from Epic), the archiver shows its window (Credits,
Help, Settings; Extract, Create and Browse Archive) on Vulkan at 16:9 until first frame + 30 s.

## Football Manager 2022: the game's ucrtbase.dll

Football Manager 2022 ships its own x64 `ucrtbase.dll` beside `fm.exe`. The emulator
(`xtajit64.dll`) imports `ucrtbase.dll` and is loaded with the default search path, which starts
at the game's folder, so the game's copy became the emulator's CRT; the loader called its x64
entry point as ARM64EC code before the emulator ran:

```text
[jit-pool] x64 image 0x73fd6f0000+0x100000 (L"\\??\\C:\\Games\\FootballManager2022\\ucrtbase.dll"): no pool copy
[exc-disp] raise tid=002c code=c0000005 flags=0 addr=0x73fd705ac0 pc=73fd705ac0 nparams=2 p0=8 p1=73fd705ac0
session: title 1: ended, exit code c0000005 after 173 ms
```

`patches/wine-pe/0034` adds `ucrtbase.dll` to the names opened from system32 as a KnownDLL would
be (beside the XInput DLLs of 0007 and 0026). Windows 10 and later also always use the system
ucrtbase.

## Runs (Vulkan, 16:9)

| IPA | Title | Result |
| --- | --- | --- |
| `efdd60bb…` | Resource Archiver, game not installed | first frame +3.85 s, exit `0x00000001` (the data error above) |
| `efdd60bb…` | Resource Archiver, game installed | first frame +4.06 s, its window on the screenshot at +30 s |
| `efdd60bb…` | Football Manager 2022 | exit `c0000005` after 173 ms (the game's ucrtbase) |
| `62b62a7c…` (with 0034) | Football Manager 2022 | past the loader: about 60 modules (EOS SDK, steam_api64, OpenAL) load and the main thread runs; no window or first frame in 15 minutes, black screen, thermal state serious |
| `62b62a7c…` (with 0034) | Resource Archiver | first frame +4.16 s, its window on the screenshot at +30 s |

With 0034, the game's main thread spends its time in `fm.exe` (a 445 MB executable), which
changes single pages of its own image to `PAGE_EXECUTE_READ` one after another (22 498
`NtProtectVirtualMemory` calls in 15 minutes, each of which rescans the JIT mappings). This looks
like an anti-tamper layer that decrypts code page by page. It is not fixed here.

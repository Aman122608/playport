# 2026-10-11: Wine's programs in system32, prerequisite reruns per build, Steamworks Shared in Storage (PLA-165, PLA-171)

**Linear:** [PLA-165](https://linear.app/playportdev/issue/PLA-165),
[PLA-171](https://linear.app/playportdev/issue/PLA-171),
[PLA-158](https://linear.app/playportdev/issue/PLA-158) (the HasRunKey write it left unproven).
Decisions [0082](../decisions/0082-system32-programs.md) and
[0083](../decisions/0083-prerequisite-rerun-per-build.md).

## Cause (PLA-165)

The problem was the build, not the staging. Wine's `configure` (`aclocal.m4`
`wine_fn_config_makefile`) puts every `programs/` directory with no explicit `enable_`
default into `arm64ec_DISABLED_SUBDIRS`. Only `cmd`, `dllhost`, `dpnsvr`, `dxdiag`, `msiexec`,
`netsh`, `regsvr32` and `rundll32` name arm64ec. The rule of 0070/0071 staged exactly what
`build/generated/wine-pe-arm64ec-windows.tsv` held, and `link_system32` linked that set.

The aarch64 builds of the other programs were in `sysarm64`. That farm is read only by a WoW64
process's native loader (wine-pe 0015), so an x86-64 caller found none of them. Earlier phone
logs show it: `[main-exe] builtin-path retry: find_builtin_dll(search_machine=0xaa64) =
0xc0000135 Machine=0x8664` and `wine: failed to start L"\??\C:\windows\system32\start.exe":
c0000135`.

## The change

- `build/stages/wine-pe.sh` configures the arm64ec tree with `--enable-<program>` for each
  `programs/` entry of the tree's `configure`, 109 options. Left out are the 16-bit programs,
  `winetest`, `winedbg`, `wineboot` and `explorer` (decision 0082 gives the reasons).
- The build made 101 more arm64ec programs, all `IMAGE_FILE_MACHINE_ARM64EC` in the manifest's
  machine check. They are staged by the existing rule. The registry seed is unchanged.
- **Bytes:**
  - +101 files and +68,616,192 bytes in the bundle.
  - The deflated dev IPA went from 394,741,991 bytes (`bc6ecde2…`, the previous commit's tree)
    to 398,155,580 bytes (`af249291…`): **+3,413,589 bytes**. This includes the Swift change.
  - The verify step reports the bundle at 1,907.5 MB.

## Host checks

- PlayportKit `PrerequisitesTests`:
  - `testEachIdRunsOncePerPrefixAndBuild`
  - `testAFailureRunsOnceMoreWithANewAppBuild`
  - `testASuccessIsNeverRunAgain` (0 and 3010)
  - `testARecordFromBeforeBuildsWereKeptRerunsItsFailuresOnce`, which decodes a record in the
    PLA-158 format with no `build`
- `./pp test` passed in full, Swift included.
- `pp build` ran `verify-ipa` with 80 checks passed.

## Phone (iPhone, dev app)

Unattended mode. Run directories are under `.work/ui-runs/`.

### IPA `5336941611f660489811b948ea141f3fc8c4d93de1dcaad0129f28f62eb2a616` (both changes)

**Hollow Knight and Portal 2.** Hollow Knight reached its first frame at +9.99 s and showed the
main menu (`20261011T103706`). Portal 2 reached its first frame at +5.85 s and showed the
Source splash (`20261011T103756`). `system32: 751 links` (it was 650).

**Counter-Strike (10), first Play (`20261011T103957`).** Its two VC++ 2019 entries had failed
under PLA-158's build. They ran again:

```
title: prerequisite: Steam x86 14.28.29334.0 (...) failed with another build (...|x86 14.28.29334.0 exit 0x1 by build unknown); build b1dd1bfe39b4c558 runs it again: C:\Games\Steamworks Shared\_CommonRedist\vcredist\2019\Microsoft Visual C++ 2019 x86.cmd
```

`cmd` started `conhost.exe` (it loaded, and the console window showed the `.cmd`'s caption).
Then `start.exe /w` started `VC_redist.x86.exe`, and WiX Burn's clean-room copy ran. Both
entries ended with **0x666 (1638)** and were recorded by build `b1dd1bfe39b4c558`.

Burn's log (`dd_vcredist_amd64_*.log`, pulled from `C:\windows\temp`) says: `Detected related
bundle: {…}, type: Upgrade, scope: PerMachine, version: 14.38.33130.0, operation: Downgrade`.
House of Golf 2's Unreal Engine prerequisite had installed VC++ 14.38 into the shared prefix,
so the 14.28 installer refuses to downgrade. Steam's `.cmd` maps only 3010 to 0, so this is
recorded as a failure. It is not counted as a success (decision 0083). This is a data point
for PLA-169 (shared versus per-title prefixes).

**Counter-Strike, second Play (`20261011T104307`).** `already ran in this prefix (… exit 0x666
… by this build); skipped`, for both entries.

**x86-64 caller probe (`20261011T104533`).** This was a scratch import of a 68 KB x86-64
console program, removed afterwards. It started each command with `CreateProcess` and printed
the exit code:
- `cmd /c start /w "" cmd /c exit 7` gave 7 (`start /w` passes the code through).
- `cmd /c start /w /b "" reg query "HKLM\…\Windows NT\CurrentVersion" /v CurrentBuildNumber`
  gave 0.
- `reg query … /v ProductName` gave 0 and printed `ProductName REG_SZ Windows 10 Pro`.
- `reg query HKLM\Software\Valve\Steam\Apps\CommonRedist /s` gave 0 and listed the seed's .NET
  and XNA keys.
- `where cmd.exe` gave 0 (`C:\windows\system32\cmd.exe`).
- `xcopy /?` and `setx /?` gave 0.
- `taskkill /im nothing-here.exe` gave 128.

The probe's own console came from `conhost.exe` (console window `0x10030`), which the screen
showed as a guest window. Pool: 15 children took `children_mb=20` live, and the head reached
347 MiB of 512.

**Lossless Scaling (993090), the HasRunKey proof.** It lists only 228989 (VC++ 2022) and 229007.
- **First Play (`20261011T104828`):**
  - `Steamworks Common Redistributables [228988, 228989, 228990, 229007] … 268907946 bytes
    downloaded in 13.0 s`
  - x86 14.51.36247.0 `exit 0x00000000 after 4 s; recorded … by build b1dd1bfe39b4c558`, then
    `wrote system Software\Valve\Steam\Apps\CommonRedist\vcredist\2022 "x86 14.51.36247.0"
    (=1)`
  - The run's `--until "mark:first window+20"` ended the app while the x64 entry was running.
    The first window was the `.cmd`'s console. That run was not recorded, so it ran again.
- **Second Play (`20261011T104946`, `--until "mark:game started+20"`):**
  - `registry playport-registry-system.reg: 1 of 1 seed keys added`. The x86 key merged at
    runtime start.
  - x64 `exit 0x00000000 after 4 s`, and its HasRunKey was written.
  - The program itself then exits `0x80008089`. It is a .NET 8 desktop app and is not a game
    (see below).
- **Third Play (`20261011T105046`):**
  - `… holds "x86 14.51.36247.0"=1 in the prefix; already run`. This is Steam's own check
    against `system.reg`.
  - `registry playport-registry-system.reg: 0 of 1 seed keys added, 1 values added to 1 present
    keys`
  - x64 `already ran in this prefix … skipped`
- The pulled `prefix/system.reg` has:

  ```
  [Software\\Valve\\Steam\\Apps\\CommonRedist\\vcredist\\2022] 1791708607
  "x64 14.51.36247.0"=dword:00000001
  "x86 14.51.36247.0"=dword:00000001
  ```

  `playport-registry-system.reg` holds the same two values under its top-up marker, and
  `playport-prerequisites.json` records both with `"build": "b1dd1bfe39b4c558"`.

**Settings › Storage.**
- `20261011T105252` showed `Steamworks Shared` after the games at 275 MB, with A "Remove".
- `20261011T105402`: A logged `steam: removing Games/Steamworks Shared …` and then `… removed`.
  The row went and free space went from 14.7 to 15.0 GB.
- The next Lossless Scaling Play (`20261011T105515`) downloaded `[228989, 229007]` again, 143 MB
  in 6.1 s. Both entries were `already run` by their HasRunKey.
- Lossless Scaling, Counter-Strike and the probe were uninstalled through the UI afterwards.

### Committed tree, IPA `af249291926c44b7f8f5233cab0a17dcf8ed0c84aa881a8db4a53ab97284e7d2`

| Title | First frame | Screen | Run |
| --- | --- | --- | --- |
| Hollow Knight | +10.07 s | main menu | `20261011T110929` |
| Portal 2 | +5.83 s | Source splash | `20261011T111020` |
| House of Golf 2 | +7.47 s | black after the splash, as before; UE prerequisite `already ran … skipped` | `20261011T111103` |

Settings › Storage showed `Steamworks Shared`, 143 MB, "Steam's redistributables, downloaded
again when needed" (`20261011T111147`). Remove took it away (`20261011T111249`). The phone has
no `Steamworks Shared` now.

## Not checked

- An x86-64 title crashing. `winedbg` is still absent from `system32`, so this is unchanged.
- Castle Crashers Demo's DXSETUP rerun. It was not installed, and its Managed DirectX failure is
  a separate issue. Its old record (no build) will run once more at its next Play.
- A console title started directly. It would now show a conhost window before its first frame.

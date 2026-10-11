# 2026-10-11: Steam install scripts and the Common Redistributables from Play (PLA-158)

**Linear:** [PLA-158](https://linear.app/playportdev/issue/PLA-158). Decisions
[0081](../decisions/0081-steam-common-redistributables.md) (with its implementation notes) and
[0079](../decisions/0079-prerequisites-from-play.md).

## What Steam does (workstation reference)

- Steam's `steamapps/common/Steamworks Shared` holds app 228980's depots, one folder each under
  `_CommonRedist` (VC++ 2012–2022, DirectX June 2010, .NET 4.8), each with an `installscript.vdf`;
  `appmanifest_228980.acf` lists them per depot as `InstallScripts`, and a game's app manifest lists
  the 228980 depots it uses as `SharedDepots`. In the depot manifests those files carry file flag
  256 (InstallScript).
- A game's Proton prefix after Steam's first run holds the HasRunKey values Steam wrote, one DWORD 1
  named after each entry (`"x64 12.0.30501"`, `"dxsetup"`) under
  `Software\Wow6432Node\Valve\Steam\Apps\CommonRedist\...`, plus the .NET and XNA ones Proton's
  `wine.inf` writes so that Steam never runs those installers.
- VC++ 2015 and later run through a `.cmd` file (`start /w "" "%~dp0\VC_redist.x64.exe" /q /norestart`);
  older VC++, DirectX, OpenAL run the installer directly.

## Host checks

- PlayportKit `SteamInstallScriptTests` (real scripts: DiRT 3's OpenAL `oalinst.exe /s`, VC++ 2022
  with HasRunKey and `Requirement_OS`, DirectX June 2010, .NET 4.8's Windows XP entry, Red Dead
  Redemption's `Registry`): parsing, the requirement rule, ids, the prerequisites and their
  HasRunKey, the already-run check against a hive, missing files, the registry file's format.
- SteamClientKit `CommonRedistributablesTests`: the depots a record lists, the flagged install scripts
  of retained manifests, which depots are missing.
- `app/tests/prefix_registry_test.c`: the registry file merges into a saved hive (missing values
  added, held ones kept, idempotent). `tools/tests/test_prefix_registry.py`: the seed's Proton keys.
- `pp test` passed. The downloader cannot run against Steam on the workstation (no Linux CM transport).

## Phone

Dev IPA sha256 `bc6ecde21e57ba571597d2fb9d6503f5be644f6835e7da178c75f819703d3f38`.

| Run | What it showed |
| --- | --- |
| Castle Crashers Demo (207100), first Play | lists 228990 (DirectX June 2010): `steam: Steamworks Common Redistributables [228990] in Games/Steamworks Shared, 102931551 bytes downloaded in 3.2 s`; prerequisite `DXSETUP.exe /silent` ran and ended `0xfffffff7` after 1 s (recorded; the game reached its first frame at +5.65 s) |
| same, second Play | no download; `already ran in this prefix (...DirectX\Jun2010|dxsetup exit 0xfffffff7 ...); skipped`; first frame +4.20 s |
| Counter-Strike (10), first Play | lists 228988 (VC++ 2019): the install grew to `[228988, 228990]` (125852268 bytes, both depots fetched again, 3.7 s); both `.cmd` entries ran and ended 1 at once |
| Hollow Knight, Portal 2 | first frame +8.94 s, +5.89 s; no install-script lines |

Why the installers failed, from their own logs: DXSETUP's `DirectX.log` ends at the Managed DirectX
check (`CFusion::CFusion(): LoadLibraryShim() failed, error = 0x80070006`, then "Installation ended
with value -9"), before it copies any file; the VC++ `.cmd`'s `start` finds no
`C:\windows\system32\start.exe` (and `cmd` no `conhost.exe`): `wine: failed to start
L"\??\C:\windows\system32\start.exe": c0000135`, the gap PLA-165 tracks. So no HasRunKey was
written on the phone; the success path (value into `playport-registry-system.reg`, merged at the
next start) is covered by the host tests only. Both titles were uninstalled afterwards; the 228
MB `Games/Steamworks Shared` stays (shared, not shown in the library). The record keeps those
failures, so after PLA-165 those entries run again only in a new prefix or with the record entry
removed.
